# Contribuindo — CriptoHost NerdOS

Obrigado pelo interesse! Este projeto é GPL-3.0 (código) com marca protegida (ver [BRANDING.md](BRANDING.md)).

## Como contribuir

1. Abra uma issue antes de PRs grandes — os milestones M0–M4 estão em [docs/PLANEJAMENTO-ESCOPO.md](docs/PLANEJAMENTO-ESCOPO.md).
2. Fork → branch (`feat/minha-feature`) → PR contra `master`.
3. O CI precisa passar (build dos 3 targets Tier 1).

## Build local

```bash
pip install platformio
pio run -e ch-devkit-v1            # firmware
pio run -e ch-devkit-v1 -t buildfs # dashboard (LittleFS)
pio run -e ch-devkit-v1 -t upload && pio run -e ch-devkit-v1 -t uploadfs
```

## Regras do código

- **Core de mineração** (`src/mining.cpp`, `src/stratum.cpp`, `src/ShaTests/`): mudanças mínimas e justificadas — é o coração herdado do NerdMiner_v2.
- **Camada CH** (`src/ch/`): todo código novo vai aqui, sempre guardado por `#ifdef CH_BUILD` para não afetar os envs upstream.
- **UI** (`data/`): nenhuma cor/fonte fora dos tokens de `data/ch-theme.css` (O7 do escopo).
- **Contrato `/api/*`**: é invariante (regra de ouro do escopo §6/M4). Mudanças de contrato exigem RFC em issue.
- Labels de issue: `type:` (feat/chore/docs/test/port), `area:` (mining/web/fleet/ota/config/brand/ci/idf), `tier:` (1/2/3).

## Testes

- `test/` contém verificações de host (vetores SHA). Rode `python3 test/test_sha_vectors.py`.
- Mudanças no pipeline de mineração exigem validação em pool real (24 h) antes do merge — descreva o setup na PR.

## Marca

PRs não podem adicionar assets de terceiros nem remover a atribuição ao upstream. Forks devem seguir o BRANDING.md.
