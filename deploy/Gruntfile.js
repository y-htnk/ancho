module.exports = function(grunt) {

  var path = require('path');
  var _s = require('underscore.string');

  var output_dir = grunt.option('outputDir') || '../build';

  var CODE_DIR = '../code/firefox/';
  var TEST_EXTENSION_DIR = '../tests/test-suite-extension';
  var MINIFIED_MODULES = 'minified_modules';

  function buildModuleList(pkg) {
    var moduleList = [];
    for (var dependency in pkg.dependencies) {
      moduleList.push(path.join(path.join('node_modules', dependency), 'package.json'));
    }
    return moduleList;
  }

  var pkg = grunt.file.readJSON('package.json');

  // Project configuration.
  grunt.initConfig({
    pkg: pkg,
    package_minifier: {
      node_modules: {
        src: buildModuleList(pkg),
        dest: MINIFIED_MODULES
      }
    },
    zip: {
      xpi: {
        router: function (filepath) {
          if (_s.startsWith(filepath, CODE_DIR)) {
            return path.relative(CODE_DIR, filepath);
          }
          else if (_s.startsWith(filepath, TEST_EXTENSION_DIR)) {
            return path.join('chrome-ext/test', path.relative(TEST_EXTENSION_DIR, filepath));
          }
          else if (_s.startsWith(filepath, MINIFIED_MODULES)) {
            return path.join('js/node_modules', path.relative(MINIFIED_MODULES, filepath));
          }
          else {
            return filepath;
          }
        },
        src: [
          path.join(CODE_DIR, 'bootstrap.js'),
          path.join(CODE_DIR, 'chrome.manifest'),
          path.join(CODE_DIR, 'install.rdf'),
          path.join(CODE_DIR, 'html/**/*'),
          path.join(CODE_DIR, 'js/**/*'),
          path.join(CODE_DIR, 'modules/**/*'),
          path.join(CODE_DIR, 'xul/**/*'),
          path.join(TEST_EXTENSION_DIR, '/**/*'),
          path.join(MINIFIED_MODULES, '/**/*')
        ],
        dest: path.join(output_dir, 'ancho-firefox-<%= pkg.version %>.xpi')
      }
    }
  });

  grunt.loadNpmTasks('grunt-package-minifier');
  grunt.loadNpmTasks('grunt-zip');

  // Default task(s).
  grunt.registerTask('default', ['package_minifier', 'zip']);
};