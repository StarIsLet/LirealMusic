// SPDX-License-Identifier: AGPL-3.0-or-later
// Lireal Music WebGPU effect shader pack.

struct AudioUniforms {
    time: f32,
    rms: f32,
    bass: f32,
    mid: f32,
    treble: f32,
    beat: f32,
    stereo_width: f32,
    drop: f32,
    width: f32,
    height: f32,
};

@group(0) @binding(0) var<uniform> audio: AudioUniforms;
@group(0) @binding(1) var frame_sampler: sampler;
@group(0) @binding(2) var frame_tex: texture_2d<f32>;

struct VertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn fullscreen_vs(@builtin(vertex_index) vertex_index: u32) -> VertexOut {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0),
    );
    var out: VertexOut;
    out.position = vec4<f32>(positions[vertex_index], 0.0, 1.0);
    out.uv = out.position.xy * 0.5 + vec2<f32>(0.5, 0.5);
    out.uv.y = 1.0 - out.uv.y;
    return out;
}

fn hash21(p: vec2<f32>) -> f32 {
    let q = vec2<f32>(dot(p, vec2<f32>(127.1, 311.7)), dot(p, vec2<f32>(269.5, 183.3)));
    return fract(sin(q.x + q.y) * 43758.5453);
}

fn spectrum_ring(uv: vec2<f32>) -> vec3<f32> {
    let center = vec2<f32>(0.27, 0.57);
    let aspect = vec2<f32>(audio.width / max(audio.height, 1.0), 1.0);
    let p = (uv - center) * aspect;
    let r = length(p);
    let a = atan2(p.y, p.x);
    let wave = 0.5 + 0.5 * sin(a * 48.0 + audio.time * 3.0 + audio.bass * 8.0);
    let ring = smoothstep(0.225 + wave * 0.045, 0.222 + wave * 0.045, r) * smoothstep(0.166, 0.174, r);
    return vec3<f32>(1.0, 0.70 + audio.treble * 0.22, 0.95) * ring * (0.25 + audio.beat * 0.75);
}

fn particles(uv: vec2<f32>) -> vec3<f32> {
    var color = vec3<f32>(0.0);
    for (var i: i32 = 0; i < 96; i = i + 1) {
        let seed = f32(i) + 1.0;
        let px = hash21(vec2<f32>(seed, 2.0));
        let py = fract(hash21(vec2<f32>(seed, 7.0)) + audio.time * (0.028 + f32(i % 7) * 0.004));
        let pos = vec2<f32>(px + sin(audio.time + seed) * 0.018 * audio.rms, py);
        let d = distance(uv, pos);
        let sparkle = smoothstep(0.012, 0.0, d) * (0.25 + audio.beat * 0.75);
        color += vec3<f32>(1.0, 0.90, 0.98) * sparkle;
    }
    return color;
}

@fragment
fn effects_fs(in: VertexOut) -> @location(0) vec4<f32> {
    let base = textureSample(frame_tex, frame_sampler, in.uv).rgb;
    let halo = spectrum_ring(in.uv);
    let snow = particles(in.uv) * 0.36;
    let bloom_tint = vec3<f32>(1.0, 0.82, 0.96) * audio.drop * 0.08;
    return vec4<f32>(base + halo + snow + bloom_tint, 1.0);
}
