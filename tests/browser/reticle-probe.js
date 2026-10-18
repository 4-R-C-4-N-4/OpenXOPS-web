// Run after starting the game in a local OPENXOPS_TEST_HOOKS=1 build.
(() => {
  if (typeof Module._qa_reticle_pixels !== 'function') throw Error('Not a QA build');
  const result = {withoutFog: Module._qa_reticle_pixels(0), withFog: Module._qa_reticle_pixels(1)};
  if (result.withoutFog !== 360 || result.withFog !== 360) {
    throw Error('Production reticle pixel regression: '+JSON.stringify(result));
  }
  return result;
})()
