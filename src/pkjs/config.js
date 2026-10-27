var defaults = require('./defaults');
var defaultColors = ['0', '1', '1', '2', '2', '3', '4'];

function minuteOptions(max) {
  var options = [];
  for (var minute = 1; minute <= max; minute++) {
    options.push({
      label: minute + ' min',
      value: String(minute)
    });
  }
  return options;
}

var config = [
  { type: 'heading', defaultValue: 'Minute Thé' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Apparence' },
      {
        type: 'toggle',
        messageKey: 'themeLight',
        label: 'Fond blanc',
        description: 'Désactivé : fond noir. Activé : fond blanc.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'Une vibration à la première alerte, deux en fin d’infusion. Pour une seule alerte finale, choisissez deux durées identiques.'
  }
];

defaults.forEach(function(t, i) {
  var n = i + 1;

  config.push({
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Thé ' + n },
      {
        type: 'toggle',
        messageKey: 'tea' + n + 'Enabled',
        label: 'Activer ce thé',
        defaultValue: t.enabled
      },
      {
        type: 'input',
        messageKey: 'tea' + n + 'Name',
        label: 'Nom',
        defaultValue: t.name
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Color',
        label: 'Couleur du bandeau',
        defaultValue: defaultColors[i],
        options: [
          { label: 'Blanc', value: '0' },
          { label: 'Vert', value: '1' },
          { label: 'Noir', value: '2' },
          { label: 'Ambré', value: '3' },
          { label: 'Jaune', value: '4' }
        ]
      },
      {
        type: 'input',
        messageKey: 'tea' + n + 'Content',
        label: 'Préparation',
        defaultValue: t.content
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Min',
        label: 'Première alerte',
        defaultValue: String(t.min),
        options: minuteOptions(15)
      },
      {
        type: 'select',
        messageKey: 'tea' + n + 'Max',
        label: 'Fin d’infusion',
        defaultValue: String(t.max),
        options: minuteOptions(20)
      }
    ]
  });
});

config.push({ type: 'submit', defaultValue: 'Enregistrer' });

module.exports = config;
