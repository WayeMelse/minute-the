var getDefaults = require('./defaults');
var i18n = require('./i18n');

module.exports = function() {
var defaults = getDefaults();
var t = i18n.get();

function minuteOptions(max) {
    var options = [];
    for (var seconds = 30; seconds <= max * 60; seconds += 30) {
      var minutes = Math.floor(seconds / 60);
      var remainder = seconds % 60;
      options.push({
        label: minutes + ':' + (remainder === 0 ? '00' : '30'),
        value: String(seconds / 60)
      });
    }
    return options;
  }

var config = [
  { type: 'heading', defaultValue: 'Minute Thé' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: t.appearance },
      {
        type: 'toggle',
        messageKey: 'themeLight',
        label: t.light,
        description: t.themeHelp,
        defaultValue: false
      }
    ]
  },
  {
    type: 'text',
    defaultValue: t.alerts
  }
];

config.push({
    type: 'section',
    items: [{
      type: 'toggle',
      messageKey: 'soundEnabled',
      label: t.sound,
      description: t.soundHelp,
      defaultValue: localStorage.getItem('soundEnabled') === '1'
    }]
  });

  defaults.forEach(function(tea, i) {
  var n = i + 1;

  config.push({
    type: 'section',
    items: [
      { type: 'heading', defaultValue: t.tea + ' ' + n },
      {
        type: 'toggle',
        messageKey: 'tea' + n + 'Enabled',
        label: t.enabled,
        defaultValue: tea.enabled
      },
      {
        type: 'input',
        messageKey: 'tea' + n + 'Name',
        label: t.name,
        defaultValue: tea.name
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Color',
        label: t.color,
        defaultValue: String(tea.color),
        options: [
          { label: t.white, value: '0' },
          { label: t.green, value: '1' },
          { label: t.black, value: '2' },
          { label: t.amber, value: '3' },
          { label: t.yellow, value: '4' }
        ]
      },
      {
        type: 'input',
        messageKey: 'tea' + n + 'Content',
        label: t.preparation,
        defaultValue: tea.content
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Min',
        label: t.first,
        defaultValue: String(tea.min),
        options: minuteOptions(15)
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Max',
        label: t.end,
        defaultValue: String(tea.max),
        options: minuteOptions(15)
      }
    ]
  });
});

config.push({ type: 'submit', defaultValue: t.save });

return config;
};
