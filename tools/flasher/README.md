# Web flasher — nerdos.cripto.host

Página estática (GitHub Pages) usando [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

## Publicação

1. Baixe da Release o `*-factory.bin` (bootloader+partitions+app) e o `*-littlefs.bin` de cada target.
2. Faça o merge do dashboard na imagem factory (offset da partição `spiffs` em `partitions/ch_4mb_ota.csv` = `0x390000`):

```bash
esptool.py --chip esp32 merge_bin -o firmware/criptohost-nerdos-ch-devkit-v1-factory.bin \
  0x0 ch-devkit-v1_factory.bin 0x390000 littlefs.bin
```

3. Coloque em `firmware/` com os nomes dos manifests.
3. Publique este diretório no GitHub Pages e aponte o CNAME `nerdos.cripto.host`.

O flash exige Chrome/Edge (Web Serial). Após flashar, o dispositivo abre o AP `CriptoHostNerdOS-XXXX` (único por placa).
