module.exports = function(grunt) {

  var path = require('path');

  var output_dir = grunt.option('outputDir') || '../build';

  var CODE_DIR = '../code/firefox/';
  var TEST_EXTENSION_DIR = '../tests/test-suite-extension';

  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),
    zip: {
      xpi: {
        router: function (filepath) {
          console.log(filepath);
          if (0 === filepath.indexOf(CODE_DIR)) {
            return path.relative(CODE_DIR, filepath);
          }
          else if (0 === filepath.indexOf(TEST_EXTENSION_DIR)) {
            return 'chrome-ext/' + path.relative(TEST_EXTENSION_DIR, filepath);
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
          path.join(TEST_EXTENSION_DIR, '/**/*')
        ],
        dest: path.join(output_dir, 'ancho-firefox-<%= pkg.version %>.xpi')
      }
    }
  });

  // Load the plugin that provides the "uglify" task.
  grunt.loadNpmTasks('grunt-zip');

  // Default task(s).
  grunt.registerTask('default', ['zip']);

};