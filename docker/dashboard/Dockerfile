FROM node:20-alpine AS base
WORKDIR /app
ENV NEXT_TELEMETRY_DISABLED=1
RUN corepack enable

# Install deps (use pnpm)
FROM base AS deps
COPY temperature-control-dashboard/pnpm-lock.yaml temperature-control-dashboard/package.json ./
RUN pnpm i --no-frozen-lockfile

# Build app
FROM deps AS build
ARG BACKEND_URL
ARG NEXT_PUBLIC_API_BASE
ENV BACKEND_URL=${BACKEND_URL}
ENV NEXT_PUBLIC_API_BASE=${NEXT_PUBLIC_API_BASE}
COPY temperature-control-dashboard/ ./
RUN pnpm build

# Run
FROM node:20-alpine AS run
WORKDIR /app
ENV NEXT_TELEMETRY_DISABLED=1
ENV NODE_ENV=production
ENV PORT=3000
ENV BACKEND_URL=http://192.168.4.1
ENV NEXT_PUBLIC_API_BASE=http://192.168.4.1
COPY --from=build /app .
EXPOSE 3000
# Arranca Next sin depender de pnpm en runtime
CMD ["node","node_modules/next/dist/bin/next","start","-p","3000"]
