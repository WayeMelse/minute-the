module.exports = function() {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    function bindTeaVisibility(number) {
      var prefix = 'tea' + number;
      var toggle = clayConfig.getItemByMessageKey(prefix + 'Enabled');

      if (!toggle) {
        return;
      }

      function updateVisibility() {
        var enabled = toggle.get();

        ['Name', 'Content', 'Min', 'Max'].forEach(function(suffix) {
          var item = clayConfig.getItemByMessageKey(prefix + suffix);
          if (!item) {
            return;
          }

          if (enabled) {
            item.show();
          } else {
            item.hide();
          }
        });
      }

      updateVisibility();
      toggle.on('change', updateVisibility);
    }

    for (var number = 1; number <= 7; number++) {
      bindTeaVisibility(number);
    }
  });
};
