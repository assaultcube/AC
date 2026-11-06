# AssaultCube Full Web Port Plan (No ENet)

This document outlines a **full web-native port** strategy for AssaultCube with:

- Rendering: **WebGL2** and **WebGPU** (dual-backend track)
- Networking: **WebSockets** (gameplay + control plane)
- Voice chat: **WebRTC audio**
- Transport: **No ENet dependency** in the browser client

## 1) Target Architecture

### 1.1 Client Runtime (Browser)
- WASM module for core simulation/gameplay logic.
- JavaScript/TypeScript host for platform integrations:
  - Dual rendering backends (WebGL2 + WebGPU) behind a shared renderer interface
  - Input (pointer lock, keyboard, gamepad)
  - Asset loading / persistence
  - WebSocket connection manager
  - WebRTC voice stack

### 1.2 Server Runtime
- Native authoritative game server process.
- Replace ENet-based client transport path with a **WebSocket session layer**.
- Optional gateway process if you want to preserve native server internals while iterating.

### 1.3 Signaling and Presence
- WebSocket control channel handles:
  - login/session
  - lobby/room join
  - voice channel membership
  - player metadata and moderation controls

## 2) Networking Without ENet

### 2.1 WebSocket Protocol Model
- Use binary frames (`ArrayBuffer`) for compact game packets.
- Packet envelope:
  - protocol version
  - message type
  - sequence number
  - ack bitset (for selective reliability)
  - payload

### 2.2 Reliability Classes
Implement ENet-like behavior at app layer:
- **Reliable ordered**: chat, inventory/stateful commands.
- **Unreliable sequenced**: position/aim snapshots.
- **Reliable unordered** (optional): non-order-dependent control events.

### 2.3 Lag Compensation and Tick Model
- Server authoritative fixed tick (e.g., 60 Hz).
- Client sends input commands stamped with local tick.
- Server echoes authoritative state + last processed input tick.
- Client performs prediction + reconciliation.

### 2.4 Match/Server Discovery
- Replace LAN broadcast discovery with:
  - central match service list endpoint, and/or
  - region lobby shards.

## 3) Voice Chat via WebRTC

### 3.1 Topology
For solo development, start with **SFU-based group voice**:
- Easier scaling than full mesh.
- Lower uplink cost for players in larger rooms.

### 3.2 Signaling
Reuse existing WebSocket channel for WebRTC signaling:
- SDP offer/answer
- ICE candidates
- channel membership updates

### 3.3 Audio Pipeline
- Capture mic with `getUserMedia`.
- Apply browser AEC/NS/AGC constraints.
- Optional positional attenuation done client-side in WebAudio gain graph.

### 3.4 Moderation/UX Controls
- Push-to-talk, mute self, mute remote.
- Server-side role checks for room moderation commands.

## 4) Rendering Migration (WebGL2 + WebGPU)

### 4.1 Renderer Strategy
- Build a **single renderer API** with two interchangeable backends:
  - **WebGL2 backend** for broad browser/hardware compatibility and stable fallback.
  - **WebGPU backend** for modern pipeline features and better long-term performance ceiling.
- Keep gameplay and scene code backend-agnostic (`RenderDevice`, `CommandList`, `Material`, `MeshHandle`).

### 4.2 Backend Scope
- **WebGL2 path**
  - Acts as compatibility baseline.
  - Uses GLSL ES shaders and explicit state batching.
- **WebGPU path**
  - Uses WGSL shaders, pipeline objects, bind groups, and explicit buffer lifetimes.
  - Initially targets desktop-class browsers where WebGPU is available.

### 4.3 Feature Rollout Order
Implement both backends in lockstep by feature tier:
1. Clear + camera + static geometry
2. Texture/material system
3. HUD and sprites
4. Particles/decals
5. Post-processing

If a feature ships in WebGPU first, maintain a temporary WebGL2 parity issue until closed.

### 4.4 Shader and Asset Pipeline
- Author shaders with shared conventions and split outputs (GLSL + WGSL).
- Add shader validation in CI for both targets.
- Keep material definition format renderer-neutral and compile to backend-specific descriptors.

### 4.5 Runtime Selection
- Detect support at startup:
  - Prefer **WebGPU** when available and stable.
  - Auto-fallback to **WebGL2** otherwise.
- Add settings toggle for manual backend override and bug triage.

### 4.6 Performance Baseline
- Shared CPU-side culling and visibility logic.
- Backend-specific GPU optimizations:
  - WebGL2: reduce state changes, batch draw calls.
  - WebGPU: persist pipelines/bind groups and use ring-buffered dynamic uniforms.

## 5) Security Model
- Validate all gameplay messages server-side.
- Signed session tokens for websocket auth.
- Rate limiting for chat and command endpoints.
- Voice room auth tied to game session identity.

## 6) Delivery Plan (Solo-Optimized)

### Milestone A (2 weeks)
- Browser boot + input + single map render.
- WebSocket connect/login + ping.
- No combat, no voice.

### Milestone B (3-4 weeks)
- Core movement/combat replicated.
- Prediction + reconciliation stable.
- HUD and weapon rendering parity for a reduced weapon set.

### Milestone C (2-3 weeks)
- Match flow: lobby -> game -> postgame.
- Chat + moderation basics.
- WebRTC voice in party/team channels.

### Milestone D (ongoing)
- Performance tuning, anti-cheat hardening, content parity.
- Expand maps/modes and regression tooling.

## 7) Immediate Task List
1. Define binary packet schema + protocol versioning doc.
2. Implement websocket session manager (client and server).
3. Build server tick/input pipeline with reconciliation hooks.
4. Add prediction for movement + firing.
5. Implement WebRTC signaling over existing websocket channel.
6. Bring up SFU (or managed provider) for room voice.
7. Add end-to-end telemetry: RTT, jitter, packet loss, tick drift.
8. Add deterministic replay snippets for regression tests.

## 8) Non-Goals for Initial Launch
- Full feature parity with every legacy mode/map.
- Perfect visual parity with native renderer on day one.
- Browser P2P authoritative gameplay (server remains authoritative).

## 9) Recommended Repository Layout

Use a monorepo so gameplay protocol types, tooling, and CI are shared.

```
AC/
├─ source/                    # existing native engine/server code
├─ docs/
│  └─ web_port_plan.md
├─ web/
│  ├─ apps/
│  │  ├─ client/              # browser game shell (TS host + dual renderer)
│  │  │  ├─ public/
│  │  │  ├─ src/
│  │  │  │  ├─ app/
│  │  │  │  ├─ net/
│  │  │  │  ├─ voice/
│  │  │  │  ├─ input/
│  │  │  │  ├─ telemetry/
│  │  │  │  └─ renderer/
│  │  │  │     ├─ core/          # shared render graph/material interfaces
│  │  │  │     ├─ webgl2/        # WebGL2 backend + GLSL shader paths
│  │  │  │     └─ webgpu/        # WebGPU backend + WGSL shader paths
│  │  │  └─ index.html
│  │  ├─ gateway/             # optional WS<->native bridge during transition
│  │  ├─ signaling/           # WebRTC signaling/session service
│  │  └─ matchmaker/          # lobby + region allocation + server list
│  ├─ packages/
│  │  ├─ protocol/            # packet schema, serializers, versioning
│  │  ├─ shared-config/       # lint/tsconfig/eslint/prettier presets
│  │  ├─ ui/                  # reusable HUD/menu web components
│  │  └─ observability/       # metrics/logging SDK wrappers
│  ├─ wasm/
│  │  ├─ CMakeLists.txt
│  │  ├─ src/                 # C/C++ bridge code exposed to JS
│  │  └─ build/               # emscripten outputs (.wasm/.js)
│  ├─ tools/
│  │  ├─ asset-pipeline/      # texture packing, model conversion, map bake
│  │  ├─ protocol-gen/        # codegen for packet structs
│  │  └─ replay-tools/        # deterministic replay validators
│  ├─ tests/
│  │  ├─ e2e/
│  │  ├─ integration/
│  │  └─ load/
│  └─ docker/
│     ├─ local-dev/
│     └─ ci/
├─ pnpm-workspace.yaml
└─ turbo.json
```

## 10) Tooling and Bundler Choices

### 10.1 Package Manager + Monorepo
- **pnpm workspaces** for deterministic installs and disk-efficient linking.
- **Turborepo** for task orchestration (`build`, `test`, `lint`, `typecheck`) and remote/local caching.

### 10.2 Browser Client Build
- **Vite** for fast local dev server and production bundles.
- **TypeScript** in strict mode.
- **esbuild** through Vite for fast transforms.
- Keep wasm artifacts as explicit assets loaded by URL (avoid opaque inlining initially).
- Maintain backend entrypoints for `webgl2` and `webgpu`, with runtime capability switching.

### 10.3 WASM Build Pipeline
- **Emscripten + CMake** for C/C++ -> wasm output.
- Export a minimal stable C ABI for JS interop.
- Generate source maps in dev builds for mixed JS/WASM debugging.

### 10.4 Graphics Tooling (Dual Backend)
- **WGSL tooling**: `@webgpu/types` + shader lint/validation in CI.
- **GLSL tooling**: `glslangValidator` (or equivalent) for syntax checks.
- Optional shader codegen layer if you want shared source-of-truth for GLSL/WGSL later.

### 10.5 Realtime + Backend Services
- **Node.js + Fastify** (or uWebSockets.js if you need very high socket fanout) for:
  - websocket session services
  - signaling and room control plane
  - matchmaker APIs
- **Redis** for ephemeral state (presence, room membership, short-lived coordination).

### 10.6 Voice Infrastructure
- Start with a managed/known SFU stack to reduce solo ops burden:
  - **LiveKit** (self-hosted or cloud) or **mediasoup** if you want custom control.
- Keep voice authorization coupled to game session JWTs.

### 10.7 Serialization + Validation
- **FlatBuffers** or **Protobuf** for packet schema evolution.
- **zod** (TS side) for config/admin payload validation.
- Protocol package owns schema versioning and compatibility tests.

### 10.8 Quality and CI
- **Vitest** for unit tests (client/packages).
- **Playwright** for browser E2E.
- **k6** (or Artillery) for websocket/session load tests.
- **GitHub Actions** for CI, with matrix jobs:
  - wasm build
  - web lint/typecheck/tests
  - backend service tests

### 10.9 Observability
- **OpenTelemetry** traces/metrics in backend services.
- **Prometheus + Grafana** dashboards for RTT, jitter, reconnect rates, room health.
- **Sentry** for frontend/runtime error tracking.

## 11) Baseline Dev Commands (Suggested)

From repo root after introducing web workspace:

- `pnpm install`
- `pnpm turbo run dev --filter=@ac/client`
- `pnpm turbo run build`
- `pnpm turbo run test`
- `pnpm turbo run lint`

WASM target example:

- `cmake -S web/wasm -B web/wasm/build -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`
- `cmake --build web/wasm/build -j`
