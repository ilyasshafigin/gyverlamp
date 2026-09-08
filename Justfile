set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

sim command="run" *args:
    @case "{{command}}" in \
        run) cd sim/web && npm run sim ;; \
        build) cd sim/web && npm run build:wasm ;; \
        test) cd sim/web && npm run test:tools ;; \
        catalog) node sim/web/tools/render-effect.js catalog --json ;; \
        render) node sim/web/tools/render-effect.js effect {{args}} ;; \
        *) echo "Unknown sim command: {{command}}. Use: run, build, test, catalog, render" >&2; exit 2 ;; \
    esac

build *envs:
    @if [ -z "{{envs}}" ]; then \
        echo "Specify firmware environment, for example: just build lamp1_ota" >&2; exit 2; \
    else \
        for env in {{envs}}; do pio run -e "$env"; done; \
    fi

upload *envs:
    @if [ -z "{{envs}}" ]; then \
        echo "Specify firmware environment, for example: just upload lamp1_ota" >&2; exit 2; \
    else \
        for env in {{envs}}; do pio run -e "$env" -t upload; done; \
    fi

tidy env:
    @pio check -e "{{env}}" --fail-on-defect=medium
