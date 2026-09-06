# Web flasher — nerdos.cripto.host

Página estática (GitHub Pages) usando [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

## Publicação

1. Baixe da Release o `*-full.bin` de cada target — desde a v0.3.0 ele já vem com bootloader, partições, app e dashboard (LittleFS) mesclados pelo `release.yml`.
2. Renomeie para o nome que o manifest espera e coloque em `firmware/`:

```bash
cp criptohost-nerdos-v0.3.0-alpha-ch-devkit-v1-full.bin firmware/criptohost-nerdos-ch-devkit-v1-factory.bin
```

3. Atualize `"version"` nos três `manifest-*.json` para a tag da Release.
4. Publique este diretório no GitHub Pages e aponte o CNAME `nerdos.cripto.host`.

O flash exige Chrome/Edge (Web Serial). Após flashar, o dispositivo abre o AP `CriptoHostNerdOS-XXXX` (único por placa).
