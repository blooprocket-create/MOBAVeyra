# Veyra backend image. Build context is this Backend/ directory.
FROM golang:1.25 AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /out/veyra-backend ./cmd/veyra-backend

FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=build /out/veyra-backend /veyra-backend
COPY config /config
USER nonroot:nonroot
ENTRYPOINT ["/veyra-backend"]
CMD ["-config", "/config/local.json"]
