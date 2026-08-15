# Web flasher — nerdos.cripto.host

Página estática (GitHub Pages) usando [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

## Publicação

1. Baixe da Release os binários *factory* (merged: bootloader+partitions+app+littlefs) gerados pelo `post_build_merge.py`.
2. Coloque em `firmware/` com os nomes dos manifests.
3. Publique este diretório no GitHub Pages e aponte o CNAME `nerdos.cripto.host`.

O flash exige Chrome/Edge (Web Serial). Após flashar, o dispositivo abre o AP `CriptoHostAP`.
