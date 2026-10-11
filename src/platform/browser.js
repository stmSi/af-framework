addToLibrary({
  af_platform_js_dialog__deps: ['$FS', '$stringToNewUTF8', 'malloc', 'free', 'af_platform_web_dialog_complete'],
  af_platform_js_dialog: function(save, extensions, location, many) {
    var patterns = UTF8ToString(extensions).split(';').filter(function(p) { return p && p !== '*'; });
    var initial = UTF8ToString(location);
    var complete = function(paths) {
      var strings = paths.map(function(path) { return stringToNewUTF8(path); });
      var list = _malloc((strings.length + 1) * 4);
      strings.forEach(function(p, i) { HEAPU32[(list >> 2) + i] = p; });
      HEAPU32[(list >> 2) + strings.length] = 0;
      _af_platform_web_dialog_complete(list);
      strings.forEach(function(p) { _free(p); });
      _free(list);
      Module['canvas'].focus();
    };
    FS.mkdirTree('/documents');
    if (save) {
      var name = window.prompt('Save file as', initial.split('/').pop() || ('untitled.' + (patterns[0] || 'txt')));
      if (name === null || !name.trim()) { complete([]); return; }
      name = name.trim().replace(/[\\/]/g, '_');
      if (patterns.length && !name.toLowerCase().endsWith('.' + patterns[0])) name += '.' + patterns[0];
      complete(['/documents/' + name]);
      return;
    }
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = patterns.map(function(p) { return '.' + p; }).join(',');
    input.multiple = !!many;
    input.style.display = 'none';
    document.body.appendChild(input);
    input.addEventListener('cancel', function() { input.remove(); complete([]); }, {once: true});
    input.addEventListener('change', async function() {
      var paths = [];
      try {
        for (var file of input.files) {
          if (file.size > 128 * 1024 * 1024) throw new Error('File exceeds 128 MB');
          var path = '/documents/' + file.name.replace(/[\\/]/g, '_');
          if (Module['setBusy']) Module['setBusy'](true, 'Reading ' + file.name + '…');
          var bytes = await new Promise(function(resolve, reject) {
            var reader = new FileReader();
            reader.onprogress = function(event) {
              if (event.lengthComputable && Module['setBusy']) Module['setBusy'](true,
                'Reading ' + file.name + '… ' + Math.round(event.loaded * 100 / event.total) + '%');
            };
            reader.onload = function() { resolve(reader.result); };
            reader.onerror = function() { reject(reader.error); };
            reader.readAsArrayBuffer(file);
          });
          FS.writeFile(path, new Uint8Array(bytes));
          paths.push(path);
        }
      } catch (error) { console.error(error); paths = []; }
      input.remove();
      if (Module['setBusy']) Module['setBusy'](false, '');
      complete(paths);
    }, {once: true});
    input.click();
  }
});
