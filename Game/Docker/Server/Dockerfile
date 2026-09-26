# Veyra's Linux dedicated server (ADR-005 step 2, ADR-006 M3). LOCAL DEVELOPMENT ONLY.
#
# Build context: a packaged Linux server, made by
#   Game/Scripts/Package.ps1 -Target VeyraServer -Platform Linux
# The root compose.yaml builds and runs it: docker compose --profile match-server up --build

# The runtime image has no shell, so check the server's shared libraries here and fail the build if
# one is missing.
FROM debian:12-slim AS check
COPY . /srv/veyra
RUN ldd /srv/veyra/Veyra/Binaries/Linux/VeyraServer > /tmp/libraries.txt \
    && cat /tmp/libraries.txt \
    && ! grep -q "not found" /tmp/libraries.txt

# The engine refuses to run as root, and the server writes its logs and saved state under
# /srv/veyra, so the files belong to the image's nonroot user (65532).
FROM gcr.io/distroless/cc-debian12:nonroot
COPY --from=check --chown=65532:65532 --chmod=0755 /srv/veyra /srv/veyra
WORKDIR /srv/veyra
EXPOSE 7777/udp
# The packaged VeyraServer.sh needs a shell; this is the command it runs.
ENTRYPOINT ["/srv/veyra/Veyra/Binaries/Linux/VeyraServer", "Veyra"]
