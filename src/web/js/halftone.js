// The renderer.
//
// The simulation hands over one texture: an RGBA image at grid resolution where
// each channel is one ink — how much black, how much blue, how much red this
// cell wants. Everything else happens here, on the GPU, in one pass.
//
// The screen is not a grid of pixels showing a grid of cells. It is a sheet of
// paper, and the paper is printed on. Each ink gets its own halftone screen: a
// lattice of dots whose radius encodes the value, laid down at its own angle.
// That last part is not a flourish. It is how printing has solved this exact
// problem for a hundred and fifty years — if you print two screens at the same
// angle they collide into a muddy moire, so you rotate them apart and the dots
// interleave into a rosette instead. We have three species in the cyclic
// competition model and three inks to print them with, so we do what a press
// would do.
//
// Dot radius goes as the square root of the value, not linearly with it. A dot's
// optical density is proportional to its area, and area goes as r^2, so a linear
// mapping crushes the midtones into mud. This is the oldest fact in halftoning
// and it is the difference between a picture that reads and one that does not.
//
// Two separate grains, as in the reference. One perturbs the edges of the dots
// before they resolve, which is ink bleeding into the tooth of the paper. The
// other sits over the whole image, which is the grain of the film you
// photographed it with. Doing only the second is what makes most "grainy" CSS
// look like a filter rather than a print.

const VERT = `#version 300 es
void main() {
  // one triangle, no buffers: the vertex is derived from its index
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

const FRAG = `#version 300 es
precision highp float;

uniform sampler2D u_field;
uniform vec2  u_grid;        // simulation size, in cells
uniform vec2  u_res;         // canvas size, in device pixels
uniform vec2  u_pan;         // centre of the view, in cells
uniform float u_zoom;        // device pixels per cell

uniform int   u_nInks;
uniform vec3  u_ink[4];
uniform float u_angle[4];

uniform vec3  u_paper;
uniform float u_pitch;       // halftone cell size, in simulation cells
uniform float u_radius;      // dot radius scale; 1.0 = dots just touch
uniform float u_contrast;
uniform int   u_hex;
uniform int   u_gooey;
uniform float u_grainMixer;
uniform float u_grainOverlay;
uniform float u_grainSize;

out vec4 outColor;

const float HEXY = 0.8660254;   // sqrt(3)/2: the row spacing of a true hex lattice

float hash21(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}

// Bilinear value noise. Speckle, not cloud: the reference thresholds it hard,
// because dry ink on rough paper is speckle.
float vnoise(vec2 st) {
  vec2 i = floor(st), f = fract(st);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// A sigmoid, not a linear stretch. At u_contrast = 0 this is the identity; wound
// up, it drives the field towards the extremes and the halftone stops being a
// tone and becomes a threshold.
//
// It has to be renormalised at the ends, and this is not a nicety. A raw
// logistic does not send 0 to 0 — at k = 3 it sends 0 to 0.18 — so an empty cell
// would acquire a value, and therefore a dot, and the halftone would print a
// grey haze of dots over the empty parts of the dish and over the bare paper
// outside it. Anchoring the curve so that s(0) = 0 and s(1) = 1 is the
// difference between a print and a smudge.
float contrast(float v) {
  float k = 14.0 * pow(u_contrast, 1.5);
  if (k < 0.001) return v;
  float lo = 1.0 / (1.0 + exp(0.5 * k));
  float hi = 1.0 / (1.0 + exp(-0.5 * k));
  float s = 1.0 / (1.0 + exp(-k * (v - 0.5)));
  return clamp((s - lo) / max(hi - lo, 1e-5), 0.0, 1.0);
}

float inkAt(vec2 cell, int i) {
  vec2 uv = cell / u_grid;
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
  vec4 t = texture(u_field, uv);
  if (i == 0) return t.r;
  if (i == 1) return t.g;
  if (i == 2) return t.b;
  return t.a;
}

void main() {
  // screen -> world, in simulation cells
  vec2 world = (gl_FragCoord.xy - 0.5 * u_res) / u_zoom + u_pan;
  world.y = u_grid.y - world.y;   // the grid's origin is top-left; the screen's is not

  vec3 col = u_paper;

  float grain = vnoise(gl_FragCoord.xy / max(u_grainSize, 0.5));
  grain = smoothstep(0.45, 0.75, grain);

  for (int i = 0; i < 4; ++i) {
    if (i >= u_nInks) break;

    // Rotate into this ink's screen, and measure in units of the screen's pitch,
    // so that one lattice cell is one unit.
    float a = u_angle[i];
    float ca = cos(a), sa = sin(a);
    vec2 q = vec2(ca * world.x + sa * world.y, -sa * world.x + ca * world.y) / u_pitch;

    float rowH = (u_hex == 1) ? HEXY : 1.0;
    float row = floor(q.y / rowH);
    float col0 = floor(q.x - ((u_hex == 1 && mod(row, 2.0) != 0.0) ? 0.5 : 0.0));

    float cover = 0.0;
    float field = 0.0;

    // A dot may be larger than its own lattice cell, so the nearest cell is not
    // enough: we have to ask the neighbours whether any of their dots reach us.
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        float r = row + float(dr);
        float xoff = (u_hex == 1 && mod(r, 2.0) != 0.0) ? 0.5 : 0.0;
        vec2 c = vec2(col0 + float(dc) + xoff + 0.5, (r + 0.5) * rowH);

        // Where is this dot's centre on the simulation grid? Rotate back out.
        vec2 wc = vec2(ca * c.x - sa * c.y, sa * c.x + ca * c.y) * u_pitch;
        vec2 cell = vec2(wc.x, u_grid.y - wc.y);

        float v = contrast(inkAt(cell, i));
        if (v <= 0.002) continue;

        // area-linear: radius goes as sqrt(value)
        float rad = 0.5 * u_radius * sqrt(v);
        float d = distance(q, c);

        // The first grain: nudge the edge of the dot itself, so the ink looks
        // like it soaked into the paper rather than being drawn on it.
        d += (grain - 0.5) * u_grainMixer * 0.30;

        if (u_gooey == 1) {
          // Metaballs: each dot contributes a smooth falloff, the fields add,
          // and the surface is where the sum crosses a half. Where two dots are
          // close their fields bridge the gap between them and they merge, which
          // is surface tension, or wet ink, depending on your taste.
          float f = 1.0 - smoothstep(0.0, max(rad * 1.35, 0.001), d);
          field += f * f;
        } else {
          float aa = fwidth(d) + 0.001;
          cover = max(cover, 1.0 - smoothstep(rad - aa, rad + aa, d));
        }
      }
    }

    if (u_gooey == 1) {
      float aa = fwidth(field) + 0.001;
      cover = smoothstep(0.5 - aa, 0.5 + aa, field);
    }

    // Multiply, because ink is subtractive: it takes light away from the paper,
    // and two inks on the same spot take away more than either alone.
    col *= mix(vec3(1.0), u_ink[i], clamp(cover, 0.0, 1.0));
  }

  // The second grain: the film, over everything.
  col = mix(col, col * (0.82 + 0.18 * grain), u_grainOverlay);

  outColor = vec4(col, 1.0);
}`;

function compile(gl, type, src) {
  const s = gl.createShader(type);
  gl.shaderSource(s, src);
  gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
    throw new Error(gl.getShaderInfoLog(s) || 'shader compile failed');
  }
  return s;
}

// Screen angles. A single ink prints straight, so a glider still looks like a
// glider. Beyond that we use the classic separation angles, which are chosen so
// that no two screens beat against each other.
const ANGLES = [0, 15, 75, 45].map((d) => (d * Math.PI) / 180);

export class Halftone {
  constructor(canvas) {
    const gl = canvas.getContext('webgl2', {
      antialias: false,
      alpha: false,
      preserveDrawingBuffer: true, // so that saving a PNG actually gets the frame
    });
    if (!gl) throw new Error('WebGL2 is required and is not available here.');
    this.gl = gl;
    this.canvas = canvas;

    const p = gl.createProgram();
    gl.attachShader(p, compile(gl, gl.VERTEX_SHADER, VERT));
    gl.attachShader(p, compile(gl, gl.FRAGMENT_SHADER, FRAG));
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
      throw new Error(gl.getProgramInfoLog(p) || 'link failed');
    }
    this.prog = p;
    gl.useProgram(p);

    this.u = {};
    for (const name of [
      'u_field', 'u_grid', 'u_res', 'u_pan', 'u_zoom', 'u_nInks', 'u_ink',
      'u_angle', 'u_paper', 'u_pitch', 'u_radius', 'u_contrast', 'u_hex',
      'u_gooey', 'u_grainMixer', 'u_grainOverlay', 'u_grainSize',
    ]) {
      this.u[name] = gl.getUniformLocation(p, name);
    }

    this.tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.tex);
    // NEAREST: a cell is a cell. Linear filtering here would smear the state of
    // one cell into its neighbour before the halftone ever saw it, and the
    // halftone is supposed to be the only thing between the model and the eye.
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

    this.vao = gl.createVertexArray();
    this.texW = 0;
    this.texH = 0;
  }

  // `pixels` is a Uint8Array view straight into the WebAssembly heap. Nothing is
  // copied on the way here; the only copy in the whole pipeline is the driver's
  // upload to VRAM.
  upload(pixels, w, h) {
    const gl = this.gl;
    gl.bindTexture(gl.TEXTURE_2D, this.tex);
    if (w !== this.texW || h !== this.texH) {
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
      this.texW = w;
      this.texH = h;
    } else {
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    }
  }

  draw(view, style, inks, grid) {
    const gl = this.gl;
    const { width, height } = this.canvas;

    gl.viewport(0, 0, width, height);
    gl.useProgram(this.prog);
    gl.bindVertexArray(this.vao);

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.tex);
    gl.uniform1i(this.u.u_field, 0);

    gl.uniform2f(this.u.u_grid, grid.w, grid.h);
    gl.uniform2f(this.u.u_res, width, height);
    gl.uniform2f(this.u.u_pan, view.panX, view.panY);
    gl.uniform1f(this.u.u_zoom, view.zoom);

    const n = Math.min(inks.length, 4);
    gl.uniform1i(this.u.u_nInks, n);

    const flat = new Float32Array(12);
    const ang = new Float32Array(4);
    for (let i = 0; i < n; i++) {
      const c = hexToRgb(inks[i].color);
      flat[i * 3] = c[0];
      flat[i * 3 + 1] = c[1];
      flat[i * 3 + 2] = c[2];
      // With one ink there is nothing to beat against, so print it straight.
      ang[i] = n === 1 ? 0 : ANGLES[i];
    }
    gl.uniform3fv(this.u.u_ink, flat);
    gl.uniform1fv(this.u.u_angle, ang);

    const paper = hexToRgb(style.paper);
    gl.uniform3f(this.u.u_paper, paper[0], paper[1], paper[2]);
    gl.uniform1f(this.u.u_pitch, style.pitch);
    gl.uniform1f(this.u.u_radius, style.radius);
    gl.uniform1f(this.u.u_contrast, style.contrast);
    gl.uniform1i(this.u.u_hex, style.hex ? 1 : 0);
    gl.uniform1i(this.u.u_gooey, style.gooey ? 1 : 0);
    gl.uniform1f(this.u.u_grainMixer, style.grainMixer);
    gl.uniform1f(this.u.u_grainOverlay, style.grainOverlay);
    gl.uniform1f(this.u.u_grainSize, style.grainSize);

    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
}

function hexToRgb(hex) {
  const h = hex.replace('#', '');
  return [
    parseInt(h.slice(0, 2), 16) / 255,
    parseInt(h.slice(2, 4), 16) / 255,
    parseInt(h.slice(4, 6), 16) / 255,
  ];
}
