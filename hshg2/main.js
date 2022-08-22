(async function() {
  window.wasm = await WebAssembly.instantiateStreaming(fetch("hshg.wasm"), {
    imports: {
      console_log: function(ptr, len, ...data) {
        console.log(new TextDecoder().decode(new Uint8Array(window.wasm.instance.exports.memory.buffer.slice(ptr, ptr + len))), ...data);
      }
    }
  });
})();