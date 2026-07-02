set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

run target:
    @case "{{target}}" in \
        sim) just sim ;; \
        *) echo "Unknown run target: {{target}}" >&2; exit 2 ;; \
    esac

sim:
    @cd sim/web && npm run sim

build *envs:
    @if [ -z "{{envs}}" ]; then \
        pio run; \
    else \
        for env in {{envs}}; do pio run -e "$env"; done; \
    fi

upload *envs:
    @if [ -z "{{envs}}" ]; then \
        pio run -t upload; \
    else \
        for env in {{envs}}; do pio run -e "$env" -t upload; done; \
    fi
