var i18n = require('./i18n');

module.exports = function() {
  var t = i18n.get();
  var temperatures = ['80-85', '85', '82', '97', '85', '85-97', '97'];
  var minimums = [1, 3, 2, 3, 3, 3, 5];
  var maximums = [3, 3, 3, 5, 3, 5, 7];
  var colors = [0, 1, 1, 2, 2, 3, 4];
  var teas = [];

  for (var i = 0; i < 7; i++) {
    teas.push({
      name: t.teaNames[i],
      content: t.spoons[(i === 0 || i === 6) ? 0 : 1] +
        ', ' + temperatures[i] + ' °C',
      min: minimums[i],
      max: maximums[i],
      enabled: i >= 2,
      color: colors[i]
    });
  }
  return teas;
};
