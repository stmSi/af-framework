addToLibrary({
  $AFCanvas: {
    context: null,
    images: new Map(),
    color: function(p) {
      return 'rgba(' + [HEAPF32[p] * 255, HEAPF32[p + 1] * 255,
        HEAPF32[p + 2] * 255, HEAPF32[p + 3]].join(',') + ')';
    },
    transform: function(p) {
      AFCanvas.context.transform(HEAPF32[p], HEAPF32[p + 2], HEAPF32[p + 1],
        HEAPF32[p + 3], HEAPF32[p + 4], HEAPF32[p + 5]);
    },
    paint: function(address) {
      var p = address >> 2, f = HEAPF32, ctx = AFCanvas.context;
      if (f[p] === 0) return AFCanvas.color(p + 1);
      if ((f[p] === 1 && Math.hypot(f[p + 7] - f[p + 5], f[p + 8] - f[p + 6]) < 0.000001) ||
          (f[p] === 2 && Math.abs(f[p + 10] - f[p + 9]) < 0.000001)) return AFCanvas.color(p + 20);
      ctx.save();
      AFCanvas.transform(p + 12);
      var gradient;
      if (f[p] === 1) {
        gradient = ctx.createLinearGradient(f[p + 5], f[p + 6], f[p + 7], f[p + 8]);
      } else if (f[p] === 2) {
        gradient = ctx.createRadialGradient(f[p + 5], f[p + 6], f[p + 9],
          f[p + 7], f[p + 8], f[p + 10]);
      } else {
        gradient = ctx.createConicGradient(f[p + 11], f[p + 5], f[p + 6]);
      }
      for (var i = 0; i < f[p + 18]; ++i) {
        var stop = p + 19 + i * 5;
        gradient.addColorStop(f[stop], AFCanvas.color(stop + 1));
      }
      ctx.restore();
      return gradient;
    }
  },
  af_canvas_js_init__deps: ['$AFCanvas'],
  af_canvas_js_init: function() {
    AFCanvas.context = Module['canvas'].getContext('2d', {alpha: false});
    Module['afCanvasFrames'] = 0;
    return AFCanvas.context ? 1 : 0;
  },
  af_canvas_js_begin__deps: ['$AFCanvas'],
  af_canvas_js_begin: function() {
    ++Module['afCanvasFrames'];
    var ctx = AFCanvas.context;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.globalAlpha = 1;
    ctx.fillStyle = '#12141b';
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
  },
  af_canvas_js_shape__deps: ['$AFCanvas'],
  af_canvas_js_shape: function(kind, matrix, width, height, radius, commands, count,
      evenodd, fill, stroke, opacity, fillOpacity, strokeOpacity, geometry) {
    var ctx = AFCanvas.context, f = HEAPF32;
    ctx.save();
    AFCanvas.transform(matrix >> 2);
    ctx.beginPath();
    if (kind === 4) {
      for (var i = 0, p = commands >> 2; i < count; ++i, p += 8) {
        switch (HEAPU32[p]) {
          case 0: ctx.moveTo(f[p + 1], f[p + 2]); break;
          case 1: ctx.lineTo(f[p + 1], f[p + 2]); break;
          case 2: ctx.bezierCurveTo(f[p + 3], f[p + 4], f[p + 5], f[p + 6],
            f[p + 1], f[p + 2]); break;
          case 3: ctx.closePath(); break;
        }
      }
    } else if (kind === 3) {
      ctx.ellipse(width / 2, height / 2, Math.max(width / 2, 0),
        Math.max(height / 2, 0), 0, 0, Math.PI * 2);
    } else if (kind === 2) {
      ctx.roundRect(0, 0, width, height, Math.max(0, Math.min(radius, width / 2, height / 2)));
    } else {
      ctx.rect(0, 0, width, height);
    }
    ctx.globalAlpha = opacity * fillOpacity;
    ctx.fillStyle = AFCanvas.paint(fill);
    ctx.fill(evenodd ? 'evenodd' : 'nonzero');
    var g = geometry >> 2;
    if (f[g] > 0) {
      ctx.globalAlpha = opacity * strokeOpacity;
      ctx.strokeStyle = AFCanvas.paint(stroke);
      ctx.lineWidth = f[g];
      ctx.lineCap = ['butt', 'round', 'square'][f[g + 1]];
      ctx.lineJoin = ['miter', 'round', 'bevel'][f[g + 2]];
      ctx.miterLimit = Math.max(f[g + 3], 1);
      ctx.lineDashOffset = f[g + 4];
      ctx.setLineDash(Array.from(f.subarray(g + 6, g + 6 + f[g + 5])));
      ctx.stroke();
    }
    ctx.restore();
  },
  af_canvas_js_image__deps: ['$AFCanvas'],
  af_canvas_js_image: function(pixels, sourceWidth, sourceHeight, matrix, width, height,
      opacity, borders, generation, hashLow, hashHigh, linear) {
    var key = hashHigh + ':' + hashLow + ':' + sourceWidth + ':' + sourceHeight + ':' + linear;
    var image = AFCanvas.images.get(key);
    if (!image || image.generation !== generation) {
      var surface = document.createElement('canvas');
      surface.width = sourceWidth;
      surface.height = sourceHeight;
      var bytes = new Uint8ClampedArray(HEAPU8.subarray(pixels, pixels + sourceWidth * sourceHeight * 4));
      if (linear) for (var i = 0; i < bytes.length; ++i) {
        if (i % 4 === 3) continue;
        var channel = bytes[i] / 255;
        bytes[i] = (channel <= 0.0031308 ? channel * 12.92 : 1.055 * Math.pow(channel, 1 / 2.4) - 0.055) * 255;
      }
      surface.getContext('2d').putImageData(new ImageData(bytes, sourceWidth, sourceHeight), 0, 0);
      image = {surface: surface, generation: generation};
      if (AFCanvas.images.size >= 256) AFCanvas.images.clear();
      AFCanvas.images.set(key, image);
    }
    var ctx = AFCanvas.context;
    ctx.save();
    AFCanvas.transform(matrix >> 2);
    ctx.globalAlpha = opacity;
    if (borders) {
      var b = borders >> 1;
      var left = HEAPU16[b], top = HEAPU16[b + 1], right = HEAPU16[b + 2], bottom = HEAPU16[b + 3];
      var factor = Math.min(1, width / Math.max(left + right, 1), height / Math.max(top + bottom, 1));
      var sx = [0, left, sourceWidth - right, sourceWidth], sy = [0, top, sourceHeight - bottom, sourceHeight];
      var dx = [0, left * factor, width - right * factor, width], dy = [0, top * factor, height - bottom * factor, height];
      for (var y = 0; y < 3; ++y) for (var x = 0; x < 3; ++x) {
        if (sx[x + 1] > sx[x] && sy[y + 1] > sy[y]) ctx.drawImage(image.surface,
          sx[x], sy[y], sx[x + 1] - sx[x], sy[y + 1] - sy[y],
          dx[x], dy[y], dx[x + 1] - dx[x], dy[y + 1] - dy[y]);
      }
    } else ctx.drawImage(image.surface, 0, 0, width, height);
    ctx.restore();
  },
  af_canvas_js_capture__deps: ['$AFCanvas', '$FS'],
  af_canvas_js_capture: function(path) {
    var data = atob(AFCanvas.context.canvas.toDataURL('image/png').split(',')[1]);
    FS.writeFile(UTF8ToString(path), Uint8Array.from(data, function(c) { return c.charCodeAt(0); }));
  },
  af_canvas_js_particles__deps: ['$AFCanvas'],
  af_canvas_js_particles: function(matrix, vertices, count, clip, clipping) {
    var ctx = AFCanvas.context, f = HEAPF32;
    ctx.save();
    if (clipping) {
      var c = clip >> 2;
      ctx.beginPath();
      ctx.rect(f[c], f[c + 1], f[c + 2], f[c + 3]);
      ctx.clip();
    }
    AFCanvas.transform(matrix >> 2);
    for (var i = 0, p = vertices >> 2; i < count; ++i, p += 8) {
      ctx.beginPath();
      ctx.ellipse(f[p], f[p + 1], Math.max(f[p + 2] * 0.5, 0),
        Math.max(f[p + 2] * 0.5, 0), f[p + 3], 0, Math.PI * 2);
      ctx.fillStyle = AFCanvas.color(p + 4);
      ctx.fill();
    }
    ctx.restore();
  },
  af_canvas_js_destroy__deps: ['$AFCanvas'],
  af_canvas_js_destroy: function() {
    AFCanvas.images.clear();
    AFCanvas.context = null;
  }
});
