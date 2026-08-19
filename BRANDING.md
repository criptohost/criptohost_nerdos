# Política de Marca e Identidade Visual — CriptoHost NerdOS

**Titular:** Cripto Host (https://cripto.host) · **Contato:** contato@cripto.host
**Última atualização:** 14/08/2026

## 1. Princípio: código livre, marca protegida

O código-fonte do CriptoHost NerdOS é software livre sob **GPL-3.0**. A **marca não é**. Esta separação é prática consolidada em projetos open-source (Firefox/Mozilla, AxeOS/Bitaxe, Grafana) e é expressamente permitida pela GPL-3.0, seção 7(e), que autoriza termos suplementares que recusam a concessão de direitos sobre nomes comerciais e marcas.

## 2. O que está protegido (NÃO licenciado pela GPL)

- O nome **"Cripto Host"** e o nome de produto **"CriptoHost NerdOS"**, incluindo grafias e variações confundíveis (ex.: "CryptoHost NerdOS", "Cripto-Host Miner");
- O **logotipo** e símbolos gráficos da Cripto Host;
- A **identidade visual**: paleta oficial, tipografia da marca, componentes com trade dress próprio e o design system Cripto Host;
- Todos os arquivos do diretório **`/data/brand/`**;
- Domínios e canais oficiais (`cripto.host`, `nerdos.cripto.host`).

## 3. O que você PODE fazer (sem autorização)

- Usar, compilar e modificar o firmware para uso pessoal ou interno, com a marca intacta;
- Forkar e redistribuir o **código** sob GPL-3.0, **desde que remova a marca** (seção 4);
- Fazer referência nominativa honesta ("compatível com CriptoHost NerdOS", "fork do CriptoHost NerdOS") sem sugerir endosso ou afiliação;
- Criar conteúdo (vídeos, tutoriais, reviews) citando o projeto.

## 4. Obrigações ao redistribuir um fork

1. **Renomear** o produto (nome que não gere confusão com "Cripto Host"/"CriptoHost NerdOS");
2. **Remover/substituir** logo, paleta oficial aplicada como identidade, tipografia de marca e todo o conteúdo de `/data/brand/`;
3. **Manter** a licença GPL-3.0, os headers de copyright e a cadeia de atribuição (NerdMiner_v2, HAN, este projeto);
4. **Não usar** os domínios, AP name (`CriptoHostNerdOS`), service name mDNS (`_criptohost._tcp`) ou prefixo de worker (`CH-`) de forma que induza usuários a acreditar que o fork é oficial.

> Dica prática: o build aceita `-D CH_UNBRANDED` que compila com tema neutro e strings genéricas, facilitando forks em conformidade.

## 5. O que exige autorização por escrito

- Uso comercial da marca (venda de hardware pré-flashado anunciado como "CriptoHost NerdOS", merchandising, serviços);
- Uso do logo/identidade em produtos, embalagens ou materiais promocionais;
- Registro de domínios, perfis ou lojas contendo a marca.

Solicitações: **contato@cripto.host**.

## 6. Relação com projetos de terceiros

- Este projeto é derivado do **NerdMiner_v2** (GPL-3.0) e presta atribuição integral ao upstream; não reivindica marca sobre "NerdMiner".
- O termo "NerdOS" no nome do produto refere-se à categoria consolidada de sistemas da comunidade Nerd*; não reivindicamos exclusividade sobre "NerdOS" isoladamente — nossa marca é o conjunto "CriptoHost NerdOS" e os elementos visuais Cripto Host.
- O escopo funcional teve como benchmark projetos públicos da categoria home-mining; **nenhum asset, código de interface ou marca de terceiros** foi utilizado.

## 7. Aplicação

A Cripto Host reserva-se o direito de requerer a adequação de usos que violem esta política, nos termos da legislação de propriedade industrial aplicável (no Brasil, Lei 9.279/96) e das seções 7(e) e 8 da GPL-3.0. Violações de marca não afetam sua licença de código, exceto quando o uso indevido constituir também violação dos termos da GPL.

---

## Anexo A — Tokens oficiais da identidade (design system)

> Fonte: Design System Cripto Host. Os valores abaixo são a base pública mínima; o design system completo (Claude Design) é o documento canônico e prevalece em caso de divergência.

| Token | Valor | Uso |
|---|---|---|
| `--ch-bg-base` | `#0B041A` | Fundo principal (dark roxo profundo — cor oficial do site) |
| `--ch-bg-surface` | `rgba(21, 10, 46, 0.72)` | Cards e superfícies (glass) |
| `--ch-primary` | `#8B5CF6` | Ações primárias, links, destaque de hashrate |
| `--ch-accent` | `#C4B5FD` | Gradientes, estados ativos, kickers |
| `--ch-success` / `--ch-warning` / `--ch-danger` | `#34D399` / `#FBBF24` / `#F87171` | Status Mining / Temp alta / Rejected & Restart |
| `--ch-text-primary` / `--ch-text-muted` | `#F4F1FA` / `#A89BC4` | Tipografia |
| `--ch-font-display` / `--ch-font-body` / `--ch-font-mono` | Urbanist / Urbanist / JetBrains Mono | Títulos / corpo / métricas numéricas |
| `--ch-radius` / `--ch-radius-lg` | `12px` / `20px` | Bordas de botões e cards |

O arquivo de implementação é [`data/ch-theme.css`](data/ch-theme.css); nenhum componente da UI deve usar cor/fonte hardcoded fora desses tokens.
