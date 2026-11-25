# Network test image (T301): the test image of F0 (tools/docker/Dockerfile.pruebas) plus iproute2 for tc/netem.
# No source and no game files are copied in: compose.yml mounts the tree read-only. tools/nettest/run.sh builds it:
#
#   docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .        (F0, once)
#   docker compose -f tools/nettest/compose.yml build                          (this one)
ARG BASE=swcoop-pruebas
FROM ${BASE}

# (Dockerfile.pruebas already installs iproute2: this only matters if that image changes)
RUN command -v tc >/dev/null 2>&1 \
 || { apt-get update -qq \
      && apt-get install -y -qq --no-install-recommends iproute2 \
      && rm -rf /var/lib/apt/lists/*; }

WORKDIR /src
