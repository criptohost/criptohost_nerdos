#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <nvs.h>
//#include "ShaTests/nerdSHA256.h"
#include "ShaTests/nerdSHA256plus.h"
#include "stratum.h"
#include "mining.h"
#include "utils.h"
#include "monitor.h"
#include "timeconst.h"
#include "drivers/displays/display.h"
#include "drivers/storage/storage.h"
#ifdef CH_BUILD
#include "ch/ch_state.h"
#endif
#include <mutex>
#include <list>
#include <map>
#include "mining/sha_backend.h"
#include "mbedtls/sha256.h"
#include "i2c_master.h"

//10 Jobs per second
#define NONCE_PER_JOB_SW 4096
#define NONCE_PER_JOB_HW 16*1024

//#define I2C_SLAVE

//#define SHA256_VALIDATE
//#define RANDOM_NONCE
#define RANDOM_NONCE_MASK 0xFFFFC000


nvs_handle_t stat_handle;

uint32_t templates = 0;
uint32_t hashes = 0;
uint32_t Mhashes = 0;
uint32_t totalKHashes = 0;
uint32_t elapsedKHs = 0;
uint64_t upTime = 0;

volatile uint32_t shares; // increase if blockhash has 32 bits of zeroes
volatile uint32_t valids; // increased if blockhash <= target

// CH: contadores do contrato /api/status (found/sent/accepted/rejected)
volatile uint32_t ch_shares_sent = 0;
volatile uint32_t ch_shares_accepted = 0;
volatile uint32_t ch_shares_rejected = 0;
volatile uint32_t ch_hw_mismatch = 0;  // candidatos HW reprovados na conferência em software (nunca submetidos)

// Track best diff
double best_diff = 0.0;

// Variables to hold data from custom textboxes
//Track mining stats in non volatile memory
extern TSettings Settings;

IPAddress serverIP(1, 1, 1, 1); //Temporally save poolIPaddres

//Global work data 
static WiFiClient client;
static miner_data mMiner; //Global miner data (Create a miner class TODO)
mining_subscribe mWorker;
mining_job mJob;
monitor_data mMonitor;
static bool volatile isMinerSuscribed = false;
unsigned long mLastTXtoPool = millis();

int saveIntervals[7] = {5 * 60, 15 * 60, 30 * 60, 1 * 3600, 3 * 3600, 6 * 3600, 12 * 3600};
int saveIntervalsSize = sizeof(saveIntervals)/sizeof(saveIntervals[0]);
int currentIntervalIndex = 0;

bool checkPoolConnection(void) {
  
  if (client.connected()) {
    return true;
  }
  
  isMinerSuscribed = false;

  Serial.println("Client not connected, trying to connect..."); 
  
  //Resolve first time pool DNS and save IP
  if(serverIP == IPAddress(1,1,1,1)) {
    WiFi.hostByName(Settings.PoolAddress.c_str(), serverIP);
    Serial.printf("Resolved DNS and save ip (first time) got: %s\n", serverIP.toString());
  }

  //Try connecting pool IP
  if (!client.connect(serverIP, Settings.PoolPort)) {
    Serial.println("Imposible to connect to : " + Settings.PoolAddress);
    WiFi.hostByName(Settings.PoolAddress.c_str(), serverIP);
    Serial.printf("Resolved DNS got: %s\n", serverIP.toString());
    return false;
  }

  return true;
}

//Implements a socketKeepAlive function and 
//checks if pool is not sending any data to reconnect again.
//Even connection could be alive, pool could stop sending new job NOTIFY
unsigned long mStart0Hashrate = 0;
bool checkPoolInactivity(unsigned int keepAliveTime, unsigned long inactivityTime){ 

    unsigned long currentKHashes = (Mhashes*1000) + hashes/1000;
    unsigned long elapsedKHs = currentKHashes - totalKHashes;

    uint32_t time_now = millis();

    // If no shares sent to pool
    // send something to pool to hold socket oppened
    if (time_now < mLastTXtoPool) //32bit wrap
      mLastTXtoPool = time_now;
    if ( time_now > mLastTXtoPool + keepAliveTime)
    {
      mLastTXtoPool = time_now;
      Serial.println("  Sending  : KeepAlive suggest_difficulty");
      //if (client.print("{}\n") == 0) {
      tx_suggest_difficulty(client, DEFAULT_DIFFICULTY);
      /*if(tx_suggest_difficulty(client, DEFAULT_DIFFICULTY)){
        Serial.println("  Sending keepAlive to pool -> Detected client disconnected");
        return true;
      }*/
    }

    if(elapsedKHs == 0){
      //Check if hashrate is 0 during inactivityTIme
      if(mStart0Hashrate == 0) mStart0Hashrate  = time_now; 
      if((time_now-mStart0Hashrate) > inactivityTime) { mStart0Hashrate=0; return true;}
      return false;
    }

  mStart0Hashrate = 0;
  return false;
}

struct JobRequest
{
  uint32_t id;
  uint32_t nonce_start;
  uint32_t nonce_count;
  double difficulty;
  uint8_t sha_buffer[128];  // header 80 bytes + padding SHA; o backend faz midstate/bswap em prepare()
};

struct JobResult
{
  uint32_t id;
  uint32_t nonce;
  uint32_t nonce_count;
  double difficulty;
  uint8_t hash[32];
};

static std::mutex s_job_mutex;
std::list<std::shared_ptr<JobRequest>> s_job_request_list_sw;
#ifdef HARDWARE_SHA265
std::list<std::shared_ptr<JobRequest>> s_job_request_list_hw;
#endif
std::list<std::shared_ptr<JobResult>> s_job_result_list;
static volatile uint8_t s_working_current_job_id = 0xFF;

static void JobPush(std::list<std::shared_ptr<JobRequest>> &job_list,  uint32_t id, uint32_t nonce_start, uint32_t nonce_count, double difficulty,
                    const uint8_t* sha_buffer)
{
  std::shared_ptr<JobRequest> job = std::make_shared<JobRequest>();
  job->id = id;
  job->nonce_start = nonce_start;
  job->nonce_count = nonce_count;
  job->difficulty = difficulty;
  memcpy(job->sha_buffer, sha_buffer, sizeof(job->sha_buffer));
  job_list.push_back(job);
}

struct Submition
{
  double diff;
  bool is32bit;
  bool isValid;
};

static void MiningJobStop(uint32_t &job_pool, std::map<uint32_t, std::shared_ptr<Submition>> & submition_map)
{
  {
    std::lock_guard<std::mutex> lock(s_job_mutex);
    s_job_result_list.clear();
    s_job_request_list_sw.clear();
    #ifdef HARDWARE_SHA265
    s_job_request_list_hw.clear();
    #endif
  }
  s_working_current_job_id = 0xFF;
  job_pool = 0xFFFFFFFF;
  submition_map.clear();
}

#ifdef RANDOM_NONCE
uint64_t s_random_state = 1;
static uint32_t RandomGet()
{
    s_random_state += 0x9E3779B97F4A7C15ull;
    uint64_t z = s_random_state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

#endif

#ifdef CH_BUILD
static String ch_reject_side(const String& reason)
{
  String r = reason;
  r.toLowerCase();
  if (r.indexOf("low diffic") >= 0 || r.indexOf("above target") >= 0 || r.indexOf("diff too") >= 0)
    return "miner: share below pool difficulty";
  if (r.indexOf("stale") >= 0 || r.indexOf("job not found") >= 0 || r.indexOf("unknown job") >= 0)
    return "pool: stale job (template already replaced)";
  if (r.indexOf("duplicate") >= 0)
    return "miner: duplicate nonce";
  if (r.indexOf("unauth") >= 0 || r.indexOf("invalid user") >= 0 || r.indexOf("invalid worker") >= 0)
    return "config: worker/wallet rejected";
  if (r.indexOf("not subscribed") >= 0 || r.indexOf("not authorized") >= 0)
    return "stratum: session not ready";
  if (r.indexOf("ntime") >= 0 || r.indexOf("invalid") >= 0)
    return "miner: invalid share fields";
  return "pool reply";
}
#endif

void runStratumWorker(void *name) {

// TEST: https://bitcoin.stackexchange.com/questions/22929/full-example-data-for-scrypt-stratum-client

  Serial.println("");
  Serial.printf("\n[WORKER] Started. Running %s on core %d\n", (char *)name, xPortGetCoreID());

  #ifdef DEBUG_MEMORY
  Serial.printf("### [Total Heap / Free heap / Min free heap]: %d / %d / %d \n", ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
  #endif

  std::map<uint32_t, std::shared_ptr<Submition>> s_submition_map;

#ifdef I2C_SLAVE
  std::vector<uint8_t> i2c_slave_vector;

  //scan for i2c slaves
  if (i2c_master_start() == 0)
    i2c_slave_vector = i2c_master_scan(0x0, 0x80);
  Serial.printf("Found %d slave workers\n", i2c_slave_vector.size());
  if (!i2c_slave_vector.empty())
  {
    Serial.print("  Workers: ");
    for (size_t n = 0; n < i2c_slave_vector.size(); ++n)
      Serial.printf("0x%02X,", (uint32_t)i2c_slave_vector[n]);
    Serial.println("");
  }
#endif

  // connect to pool  
  double currentPoolDifficulty = DEFAULT_DIFFICULTY;
  uint32_t nonce_pool = 0;
  uint32_t job_pool = 0xFFFFFFFF;
  uint32_t last_job_time = millis();
  String lastNotifyKey;

  while(true) {
      
    if(WiFi.status() != WL_CONNECTED){
      // WiFi is disconnected, so reconnect now
      mMonitor.NerdStatus = NM_Connecting;
      MiningJobStop(job_pool, s_submition_map);
      WiFi.reconnect();
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      continue;
    } 

    if(!checkPoolConnection()){
      //If server is not reachable add random delay for connection retries
      //Generate value between 1 and 60 secs
      MiningJobStop(job_pool, s_submition_map);
      vTaskDelay(((1 + rand() % 60) * 1000) / portTICK_PERIOD_MS);
      continue;
    }

    if(!isMinerSuscribed)
    {
      lastNotifyKey = "";
      mWorker = init_mining_subscribe();

      // STEP 1: Pool server connection (SUBSCRIBE)
      if(!tx_mining_subscribe(client, mWorker)) { 
        client.stop();
        MiningJobStop(job_pool, s_submition_map);
        continue; 
      }
      
      strcpy(mWorker.wName, Settings.BtcWallet);
      strcpy(mWorker.wPass, Settings.PoolPassword);
      // STEP 2: Pool authorize work (Block Info)
      tx_mining_auth(client, mWorker.wName, mWorker.wPass); //Don't verifies authoritzation, TODO
      //tx_mining_auth2(client, mWorker.wName, mWorker.wPass); //Don't verifies authoritzation, TODO

      // STEP 3: Suggest pool difficulty
      tx_suggest_difficulty(client, currentPoolDifficulty);

      isMinerSuscribed=true;
      uint32_t time_now = millis();
      mLastTXtoPool = time_now;
      last_job_time = time_now;
    }

    //Check if pool is down for almost 5minutes and then restart connection with pool (1min=600000ms)
    if(checkPoolInactivity(KEEPALIVE_TIME_ms, POOLINACTIVITY_TIME_ms)){
      //Restart connection
      Serial.println("  Detected more than 2 min without data form stratum server. Closing socket and reopening...");
      client.stop();
      isMinerSuscribed=false;
      MiningJobStop(job_pool, s_submition_map);
      continue; 
    }

    {
      uint32_t time_now = millis();
      if (time_now < last_job_time) //32bit wrap
        last_job_time = time_now;
      if (time_now >= last_job_time + 10*60*1000)  //10minutes without job
      {
        client.stop();
        isMinerSuscribed=false;
        MiningJobStop(job_pool, s_submition_map);
        continue;
      }
    }


    //Read pending messages from pool
    while(client.connected() && client.available())
    {
      String line = client.readStringUntil('\n');
      //Serial.println("  Received message from pool");      
      stratum_method result = parse_mining_method(line);
      switch (result)
      {
          case MINING_NOTIFY:         if(parse_mining_notify(line, mJob))
                                      {
                                          String notifyKey = mJob.job_id + mJob.ntime;
                                          if (notifyKey == lastNotifyKey)
                                            break;
                                          lastNotifyKey = notifyKey;
                                          {
                                            std::lock_guard<std::mutex> lock(s_job_mutex);
                                            s_job_result_list.clear();
                                            s_job_request_list_sw.clear();
                                            #ifdef HARDWARE_SHA265
                                            s_job_request_list_hw.clear();
                                            #endif
                                          }
                                          //Increse templates readed
                                          templates++;
                                          job_pool++;
                                          s_working_current_job_id = job_pool & 0xFF; //Terminate current job in thread
                                          mMonitor.NerdStatus = NM_hashing;

                                          last_job_time = millis();
                                          mLastTXtoPool = last_job_time;

                                          uint32_t mh = hashes/1000000;
                                          Mhashes += mh;
                                          hashes -= mh*1000000;

                                          //Prepare data for new jobs
                                          mMiner=calculateMiningData(mWorker, mJob);

                                          memset(mMiner.bytearray_blockheader+80, 0, 128-80);
                                          mMiner.bytearray_blockheader[80] = 0x80;
                                          mMiner.bytearray_blockheader[126] = 0x02;
                                          mMiner.bytearray_blockheader[127] = 0x80;

                                          #ifdef RANDOM_NONCE
                                          nonce_pool = RandomGet() & RANDOM_NONCE_MASK;
                                          #else
                                            #ifdef I2C_SLAVE
                                            if (!i2c_slave_vector.empty())
                                              nonce_pool = 0x10000000;
                                            else
                                            #endif
                                              nonce_pool = 0xDA54E700;  //nonce 0x00000000 is not possible, start from some random nonce
                                          #endif
                                          

                                          {
                                            std::lock_guard<std::mutex> lock(s_job_mutex);
                                            for (int i = 0; i < 4; ++ i)
                                            {
                                              #if 1
                                              JobPush( s_job_request_list_sw, job_pool, nonce_pool, NONCE_PER_JOB_SW, currentPoolDifficulty, mMiner.bytearray_blockheader);
                                              #ifdef RANDOM_NONCE
                                              nonce_pool = RandomGet() & RANDOM_NONCE_MASK;
                                              #else
                                              nonce_pool += NONCE_PER_JOB_SW;
                                              #endif
                                              #endif
                                              #ifdef HARDWARE_SHA265
                                                JobPush( s_job_request_list_hw, job_pool, nonce_pool, NONCE_PER_JOB_HW, currentPoolDifficulty, mMiner.bytearray_blockheader);
                                              #ifdef RANDOM_NONCE
                                              nonce_pool = RandomGet() & RANDOM_NONCE_MASK;
                                              #else
                                              nonce_pool += NONCE_PER_JOB_HW;
                                              #endif
                                              #endif
                                            }
                                          }
                                          #ifdef I2C_SLAVE
                                          //Nonce for nonce_pool starts from 0x10000000
                                          //For i2c slave we give nonces from 0x20000000, that is 0x10000000 nonces per slave
                                          i2c_feed_slaves(i2c_slave_vector, job_pool & 0xFF, 0x20, currentPoolDifficulty, mMiner.bytearray_blockheader);
                                          #endif
                                      } else
                                      {
                                        Serial.println("Parsing error, need restart");
                                        client.stop();
                                        isMinerSuscribed=false;
                                        MiningJobStop(job_pool, s_submition_map);
                                      }
                                      break;
          case MINING_SET_DIFFICULTY: parse_mining_set_difficulty(line, currentPoolDifficulty);
                                      break;
          case STRATUM_SUCCESS:       {
                                        unsigned long id = parse_extract_id(line);
                                        auto itt = s_submition_map.find(id);
                                        if (itt != s_submition_map.end())
                                        {
                                          ch_shares_accepted++;
                                          if (itt->second->diff > best_diff)
                                            best_diff = itt->second->diff;
                                          if (itt->second->is32bit)
                                            shares++;
                                          if (itt->second->isValid)
                                          {
                                            Serial.println("CONGRATULATIONS! Valid block found");
                                            valids++;
                                          }
                                          s_submition_map.erase(itt);
                                        }
                                      }
                                      break;
          case STRATUM_PARSE_ERROR:   {
                                        unsigned long id = parse_extract_id(line);
                                        auto itt = s_submition_map.find(id);
                                        String why = stratum_last_error_text();
                                        if (itt != s_submition_map.end())
                                        {
                                          ch_shares_rejected++;
                                          Serial.printf("Refuse submition %lu (%s)\n", id, why.c_str());
#ifdef CH_BUILD
                                          char buf[192];
                                          snprintf(buf, sizeof(buf),
                                                   "Share rejected — %s · %s · diff %.4g",
                                                   why.c_str(),
                                                   ch_reject_side(why).c_str(),
                                                   itt->second->diff);
                                          ch_log_event("reject", String(buf));
#endif
                                          s_submition_map.erase(itt);
                                        }
#ifdef CH_BUILD
                                        else if (why.length() && why != "no error field")
                                          ch_log_event("conn", "Stratum error — " + why);
#endif
                                      }
                                      break;
          default:                    Serial.println("  Parsed JSON: unknown"); break;

      }
    }

    std::list<std::shared_ptr<JobResult>> job_result_list;
    #ifdef I2C_SLAVE
    if (i2c_slave_vector.empty() || job_pool == 0xFFFFFFFF)
    {
      vTaskDelay(50 / portTICK_PERIOD_MS); //Small delay
    } else
    {
      uint32_t time_start = millis();
      i2c_hit_slaves(i2c_slave_vector);
      vTaskDelay(5 / portTICK_PERIOD_MS);
      uint32_t nonces_done = 0;
      std::vector<uint32_t> nonce_vector = i2c_harvest_slaves(i2c_slave_vector, job_pool & 0xFF, nonces_done);
      hashes += nonces_done;
      for (size_t n = 0; n < nonce_vector.size(); ++n)
      {
        std::shared_ptr<JobResult> result = std::make_shared<JobResult>();
        ((uint32_t*)(mMiner.bytearray_blockheader+64+12))[0] = nonce_vector[n];
        if (nerd_sha256d_baked(diget_mid, mMiner.bytearray_blockheader+64, bake, result->hash))
        {
          result->id = job_pool;
          result->nonce = nonce_vector[n];
          result->nonce_count = 0;
          result->difficulty = diff_from_target(result->hash);
          job_result_list.push_back(result);
        }
      }
      uint32_t time_end = millis();
      if (time_end > time_start)
      {
        uint32_t elapsed = time_end - time_start;
        if (elapsed < 50)
          vTaskDelay((50 - elapsed) / portTICK_PERIOD_MS);
      } else
        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
    #endif

    
    if (job_pool != 0xFFFFFFFF)
    {
      std::lock_guard<std::mutex> lock(s_job_mutex);
      job_result_list.insert(job_result_list.end(), s_job_result_list.begin(), s_job_result_list.end());
      s_job_result_list.clear();

#if 1
      while (s_job_request_list_sw.size() < 4)
      {
        JobPush( s_job_request_list_sw, job_pool, nonce_pool, NONCE_PER_JOB_SW, currentPoolDifficulty, mMiner.bytearray_blockheader);
        #ifdef RANDOM_NONCE
        nonce_pool = RandomGet() & RANDOM_NONCE_MASK;
        #else
        nonce_pool += NONCE_PER_JOB_SW;
        #endif
      }
#endif

      #ifdef HARDWARE_SHA265
      while (s_job_request_list_hw.size() < 4)
      {
        JobPush( s_job_request_list_hw, job_pool, nonce_pool, NONCE_PER_JOB_HW, currentPoolDifficulty, mMiner.bytearray_blockheader);
        #ifdef RANDOM_NONCE
        nonce_pool = RandomGet() & RANDOM_NONCE_MASK;
        #else
        nonce_pool += NONCE_PER_JOB_HW;
        #endif
      }
      #endif
    }

    if (client.available()) {
      for (auto &res : job_result_list) {
        hashes += res->nonce_count;
        res->nonce_count = 0;
      }
      std::lock_guard<std::mutex> lock(s_job_mutex);
      s_job_result_list.splice(s_job_result_list.begin(), job_result_list);
    } else while (!job_result_list.empty())
    {
      std::shared_ptr<JobResult> res = job_result_list.front();
      job_result_list.pop_front();

      hashes += res->nonce_count;
      if (res->difficulty > currentPoolDifficulty && job_pool == res->id && res->nonce != 0xFFFFFFFF)
      {
        if (!client.connected())
          break;
        unsigned long sumbit_id = 0;
        tx_mining_submit(client, mWorker, mJob, res->nonce, sumbit_id);
        ch_shares_sent++;
        Serial.print("   - Current diff share: "); Serial.println(res->difficulty,12);
        Serial.print("   - Current pool diff : "); Serial.println(currentPoolDifficulty,12);
        Serial.print("   - TX SHARE: ");
        for (size_t i = 0; i < 32; i++)
            Serial.printf("%02x", res->hash[i]);
        Serial.println("");
        mLastTXtoPool = millis();

        std::shared_ptr<Submition> submition = std::make_shared<Submition>();
        submition->diff = res->difficulty;
        submition->is32bit = (res->hash[29] == 0 && res->hash[28] == 0);
        if (submition->is32bit)
        {
          submition->isValid = checkValid(res->hash, mMiner.bytearray_target);
        } else
          submition->isValid = false;

        s_submition_map.insert(std::make_pair(sumbit_id, submition));
        if (s_submition_map.size() > 32)
          s_submition_map.erase(s_submition_map.begin());
      }
    }

#ifndef I2C_SLAVE
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif
  }
}

//////////////////THREAD CALLS///////////////////

// Um worker, N backends (§10 passo 1): o backend faz midstate/bswap em prepare() e o laço de nonce em scan().
static void minerWorker(IShaBackend& be, std::list<std::shared_ptr<JobRequest>>& queue, unsigned miner_id)
{
  Serial.printf("[MINER] %u Started minerWorker (%s)\n", miner_id, be.name());

  std::shared_ptr<JobRequest> job;
  std::shared_ptr<JobResult> result;
  uint8_t hash[32];
  while (1)
  {
    {
      std::lock_guard<std::mutex> lock(s_job_mutex);
      if (result)
      {
        if (s_job_result_list.size() < 16)
          s_job_result_list.push_back(result);
        result.reset();
      }
      if (!queue.empty())
      {
        job = queue.front();
        queue.pop_front();
      } else
        job.reset();
    }
    if (job)
    {
      result = std::make_shared<JobResult>();
      result->difficulty = job->difficulty;
      result->nonce = 0xFFFFFFFF;
      result->id = job->id;
      uint8_t job_in_work = job->id & 0xFF;
      be.prepare(job->sha_buffer);
      uint32_t n = job->nonce_start, done = 0;
      while (done < job->nonce_count)
      {
        uint32_t chunk = job->nonce_count - done;
        if (chunk > 2048) chunk = 2048;  // lote: lock do motor + checagem de job novo a cada ~4 ms
        uint32_t n0 = n;
        if (be.scan(n, chunk, hash))
        {
          double diff_hash = diff_from_target(hash);
          if (diff_hash > result->difficulty && isSha256Valid(hash))
          {
            // §10 passo 5: confere o candidato em software antes de submeter (mesmo padrão do SparkMiner).
            // Custa 1 double-SHA por acerto de 16 bits (~10/s); pega hash errado do periférico sem mandar share ruim.
            uint8_t hdr[80], inter[32], ref[32];
            memcpy(hdr, job->sha_buffer, 80); memcpy(hdr + 76, &n, 4);
            mbedtls_sha256_ret(hdr, 80, inter, 0); mbedtls_sha256_ret(inter, 32, ref, 0);
            if (memcmp(ref, hash, 32) == 0)
            {
              result->difficulty = diff_hash;
              result->nonce = n;
              memcpy(result->hash, hash, 32);
            }
            else
            {
              ch_hw_mismatch++;
#ifdef CH_BUILD
              ch_log_event("test", String(be.name()) + " hash divergente do software no nonce " + String(n, HEX) + " — share descartado");
#endif
            }
          }
          ++n;  // retoma após o candidato
        }
        done += n - n0;
        if (s_working_current_job_id != job_in_work)  // job novo na pool: abandona o resto
          break;
      }
      __atomic_fetch_add(&hashes, done, __ATOMIC_RELAXED);
      result->nonce_count = 0;
    } else
      vTaskDelay(2 / portTICK_PERIOD_MS);

    esp_task_wdt_reset();
  }
}

void minerWorkerSw(void * task_id) { minerWorker(*sha_bench.sw_sel, s_job_request_list_sw, (uint32_t)task_id); }
#ifdef HARDWARE_SHA265
void minerWorkerHw(void * task_id) { minerWorker(*sha_bench.hw, s_job_request_list_hw, (uint32_t)task_id); }
#endif


#define DELAY 100
#define REDRAW_EVERY 10

void restoreStat() {
  if(!Settings.saveStats) return;
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.printf("[MONITOR] NVS partition is full or has invalid version, erasing...\n");
    nvs_flash_init();
  }

  ret = nvs_open("state", NVS_READWRITE, &stat_handle);

  size_t required_size = sizeof(double);
  nvs_get_blob(stat_handle, "best_diff", &best_diff, &required_size);
  nvs_get_u32(stat_handle, "Mhashes", &Mhashes);
  uint32_t nv_shares, nv_valids;
  nvs_get_u32(stat_handle, "shares", &nv_shares);
  nvs_get_u32(stat_handle, "valids", &nv_valids);
  shares = nv_shares;
  valids = nv_valids;
  nvs_get_u32(stat_handle, "templates", &templates);
  nvs_get_u64(stat_handle, "upTime", &upTime);

  uint32_t crc = crc32_reset();
  crc = crc32_add(crc, &best_diff, sizeof(best_diff));
  crc = crc32_add(crc, &Mhashes, sizeof(Mhashes));
  crc = crc32_add(crc, &nv_shares, sizeof(nv_shares));
  crc = crc32_add(crc, &nv_valids, sizeof(nv_valids));
  crc = crc32_add(crc, &templates, sizeof(templates));
  crc = crc32_add(crc, &upTime, sizeof(upTime));
  crc = crc32_finish(crc);

  uint32_t nv_crc;
  nvs_get_u32(stat_handle, "crc32", &nv_crc);
  if (nv_crc != crc)
  {
    best_diff = 0.0;
    Mhashes = 0;
    shares = 0;
    valids = 0;
    templates = 0;
    upTime = 0;
  }
}

void saveStat() {
  if(!Settings.saveStats) return;
  Serial.printf("[MONITOR] Saving stats\n");
  nvs_set_blob(stat_handle, "best_diff", &best_diff, sizeof(best_diff));
  nvs_set_u32(stat_handle, "Mhashes", Mhashes);
  nvs_set_u32(stat_handle, "shares", shares);
  nvs_set_u32(stat_handle, "valids", valids);
  nvs_set_u32(stat_handle, "templates", templates);
  nvs_set_u64(stat_handle, "upTime", upTime);

  uint32_t crc = crc32_reset();
  crc = crc32_add(crc, &best_diff, sizeof(best_diff));
  crc = crc32_add(crc, &Mhashes, sizeof(Mhashes));
  uint32_t nv_shares = shares;
  uint32_t nv_valids = valids;
  crc = crc32_add(crc, &nv_shares, sizeof(nv_shares));
  crc = crc32_add(crc, &nv_valids, sizeof(nv_valids));
  crc = crc32_add(crc, &templates, sizeof(templates));
  crc = crc32_add(crc, &upTime, sizeof(upTime));
  crc = crc32_finish(crc);
  nvs_set_u32(stat_handle, "crc32", crc);
}

void resetStat() {
    Serial.printf("[MONITOR] Resetting NVS stats\n");
    templates = hashes = Mhashes = totalKHashes = elapsedKHs = upTime = shares = valids = 0;
    best_diff = 0.0;
    saveStat();
}

void runMonitor(void *name)
{

  Serial.println("[MONITOR] started");
  restoreStat();

  unsigned long mLastCheck = 0;

  resetToFirstScreen();

  unsigned long frame = 0;

  uint32_t seconds_elapsed = 0;

  totalKHashes = (Mhashes * 1000) + hashes / 1000;
  uint32_t last_update_millis = millis();
  uint32_t uptime_frac = 0;

  while (1)
  {
    uint32_t now_millis = millis();
    if (now_millis < last_update_millis)
      now_millis = last_update_millis;
    
    uint32_t mElapsed = now_millis - mLastCheck;
    if (mElapsed >= 1000)
    { 
      mLastCheck = now_millis;
      last_update_millis = now_millis;
      unsigned long currentKHashes = (Mhashes * 1000) + hashes / 1000;
      elapsedKHs = currentKHashes - totalKHashes;
      totalKHashes = currentKHashes;
      Serial.printf("[BENCH] khs=%lu hw=%s sw=%s\n", (unsigned long)elapsedKHs, sha_bench.hw->name(), sha_bench.sw_sel->name());  // lido por tools/bench/bench.py

      uptime_frac += mElapsed;
      while (uptime_frac >= 1000)
      {
        uptime_frac -= 1000;
        upTime ++;
      }

      drawCurrentScreen(mElapsed);

      // Monitor state when hashrate is 0.0
      if (elapsedKHs == 0)
      {
        Serial.printf(">>> [i] Miner: newJob>%s / inRun>%s) - Client: connected>%s / subscribed>%s / wificonnected>%s\n",
            "true",//(1) ? "true" : "false",
            isMinerSuscribed ? "true" : "false",
            client.connected() ? "true" : "false", isMinerSuscribed ? "true" : "false", WiFi.status() == WL_CONNECTED ? "true" : "false");
      }

      #ifdef DEBUG_MEMORY
      Serial.printf("### [Total Heap / Free heap / Min free heap]: %d / %d / %d \n", ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
      Serial.printf("### Max stack usage: %d\n", uxTaskGetStackHighWaterMark(NULL));
      #endif

      seconds_elapsed++;

      if(seconds_elapsed % (saveIntervals[currentIntervalIndex]) == 0){
        saveStat();
        seconds_elapsed = 0;
        if(currentIntervalIndex < saveIntervalsSize - 1)
          currentIntervalIndex++;
      }    
    }
    animateCurrentScreen(frame);
    doLedStuff(frame);

    vTaskDelay(DELAY / portTICK_PERIOD_MS);
    frame++;
  }
}

#ifdef CH_BUILD
static TaskHandle_t s_miner1 = NULL, s_miner2 = NULL, s_stratum = NULL, s_monitor = NULL;
static bool s_otaPaused = false;

void mining_register_tasks(TaskHandle_t miner1, TaskHandle_t miner2, TaskHandle_t stratum, TaskHandle_t monitor)
{
  s_miner1 = miner1;
  s_miner2 = miner2;
  s_stratum = stratum;
  s_monitor = monitor;
}

void mining_pause_for_ota()
{
  if (s_otaPaused) return;
  s_otaPaused = true;
  if (s_miner1) { esp_task_wdt_delete(s_miner1); vTaskSuspend(s_miner1); }
  if (s_miner2) { esp_task_wdt_delete(s_miner2); vTaskSuspend(s_miner2); }
  if (s_stratum) vTaskSuspend(s_stratum);
  if (s_monitor) vTaskSuspend(s_monitor);
  Serial.println("[CH] Mining paused for OTA");
}

void mining_resume_after_ota()
{
  if (!s_otaPaused) return;
  s_otaPaused = false;
  if (s_monitor) vTaskResume(s_monitor);
  if (s_stratum) vTaskResume(s_stratum);
  if (s_miner1) { vTaskResume(s_miner1); esp_task_wdt_add(s_miner1); }
  if (s_miner2) { vTaskResume(s_miner2); esp_task_wdt_add(s_miner2); }
  Serial.println("[CH] Mining resumed after failed OTA");
}
#endif
