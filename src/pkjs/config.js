var defaults = require('./defaults');
var items = [];
defaults.forEach(function(t, i) {
  var n = i + 1;
  items.push(
    { type: 'heading', defaultValue: 'Thé ' + n },
    { type: 'toggle', messageKey: 'tea' + n + 'Enabled', label: 'Activer', defaultValue: t.enabled },
    { type: 'input', messageKey: 'tea' + n + 'Name', label: 'Nom', defaultValue: t.name },
    { type: 'input', messageKey: 'tea' + n + 'Content', label: 'Préparation', defaultValue: t.content },
    { type: 'slider', messageKey: 'tea' + n + 'Min', label: 'Début (min)', min: 1, max: 15, step: 1, defaultValue: t.min },
    { type: 'slider', messageKey: 'tea' + n + 'Max', label: 'Fin (min)', min: 1, max: 20, step: 1, defaultValue: t.max }
  );
});
module.exports = [
  { type: 'heading', defaultValue: 'Minute Thé' },
  { type: 'section', items: items },
  { type: 'submit', defaultValue: 'Enregistrer' }
];
