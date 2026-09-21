set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

sim command="run" *args:
    @case "{{ command }}" in \
      run) cd sim/web && npm run sim ;; \
      build) cd sim/web && npm run build:wasm ;; \
      test) cd sim/web && npm run test:tools ;; \
      catalog) node sim/web/tools/render-effect.js catalog --json ;; \
      render) node sim/web/tools/render-effect.js effect {{ args }} ;; \
      *) echo "Unknown sim command: {{ command }}. Use: run, build, test, catalog, render" >&2; exit 2 ;; \
    esac

lamp command="build" *envs:
    @case "{{ command }}" in \
      build) \
        if [ -z "{{ envs }}" ]; then \
          echo "Specify firmware environment, for example: just lamp build lamp1_ota" >&2; exit 2; \
        else \
          for env in {{ envs }}; do pio run -e "$env"; done; \
        fi ;; \
      upload) \
        if [ -z "{{ envs }}" ]; then \
          echo "Specify firmware environment, for example: just lamp upload lamp1_ota" >&2; exit 2; \
        else \
          for env in {{ envs }}; do pio run -e "$env" -t upload; done; \
        fi ;; \
      *) echo "Unknown lamp command: {{ command }}. Use: build or upload" >&2; exit 2 ;; \
    esac

panel command="build" target="diag" port="":
    @case "{{ command }}" in \
      build) \
        case "{{ target }}" in \
          diag) pio run -d control-pad -e diag ;; \
          input) pio run -d control-pad -e input ;; \
          *) \
            if [[ "{{ target }}" =~ ^[a-z][a-z0-9_-]{0,31}$ ]] && grep -Fqx "[env:{{ target }}]" control-pad/platformio.local.ini 2>/dev/null; then \
              pio run -d control-pad -e "{{ target }}"; \
            else \
              echo "Unknown panel build target: {{ target }}. Use: diag, input, or a provisioned profile" >&2; exit 2; \
            fi ;; \
        esac ;; \
      upload) \
        if [ -z "{{ port }}" ]; then echo "Specify serial port: just panel upload <profile> <port>" >&2; exit 2; fi; \
        case "{{ target }}" in \
          diag) pio run -d control-pad -e diag -t upload --upload-port "{{ port }}" ;; \
          input) echo "panel input cannot be uploaded" >&2; exit 2 ;; \
          *) \
            if [[ "{{ target }}" =~ ^[a-z][a-z0-9_-]{0,31}$ ]] && grep -Fqx "[env:{{ target }}]" control-pad/platformio.local.ini 2>/dev/null; then \
              pio run -d control-pad -e "{{ target }}" -t upload --upload-port "{{ port }}"; \
            else \
              echo "Unknown panel upload target: {{ target }}. Use: diag or a provisioned profile" >&2; exit 2; \
            fi ;; \
        esac ;; \
      test) \
        case "{{ target }}" in \
          protocol) sh lib/ControlPadProtocol/tests/run.sh ;; \
          *) echo "Unknown panel test target: {{ target }}. Use: protocol" >&2; exit 2 ;; \
        esac ;; \
      provision) python3 control-pad/tools/provision_panel.py "{{ target }}" ;; \
      *) echo "Unknown panel command: {{ command }}. Use: build, test, provision, upload" >&2; exit 2 ;; \
    esac

tidy env:
    @pio check -e "{{ env }}" --fail-on-defect=medium
