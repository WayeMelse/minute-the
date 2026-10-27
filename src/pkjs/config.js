module.exports = [
  {
    "type": "heading",
    "defaultValue": "Minute Thé"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Thé 1"
      },
      {
        "type": "toggle",
        "messageKey": "tea1Enabled",
        "label": "Activer",
        "defaultValue": true
      },
      {
        "type": "input",
        "messageKey": "tea1Name",
        "label": "Nom",
        "defaultValue": "Thé noir"
      },
      {
        "type": "input",
        "messageKey": "tea1Content",
        "label": "Préparation",
        "defaultValue": "1 c. à c., 95 °C"
      },
      {
        "type": "slider",
        "messageKey": "tea1Min",
        "label": "Début (min)",
        "min": 1,
        "max": 15,
        "step": 1,
        "defaultValue": 3
      },
      {
        "type": "slider",
        "messageKey": "tea1Max",
        "label": "Fin (min)",
        "min": 1,
        "max": 20,
        "step": 1,
        "defaultValue": 5
      },
      {
        "type": "heading",
        "defaultValue": "Thé 2"
      },
      {
        "type": "toggle",
        "messageKey": "tea2Enabled",
        "label": "Activer",
        "defaultValue": true
      },
      {
        "type": "input",
        "messageKey": "tea2Name",
        "label": "Nom",
        "defaultValue": "Thé vert"
      },
      {
        "type": "input",
        "messageKey": "tea2Content",
        "label": "Préparation",
        "defaultValue": "1 c. à c., 80 °C"
      },
      {
        "type": "slider",
        "messageKey": "tea2Min",
        "label": "Début (min)",
        "min": 1,
        "max": 15,
        "step": 1,
        "defaultValue": 2
      },
      {
        "type": "slider",
        "messageKey": "tea2Max",
        "label": "Fin (min)",
        "min": 1,
        "max": 20,
        "step": 1,
        "defaultValue": 3
      },
      {
        "type": "heading",
        "defaultValue": "Thé 3"
      },
      {
        "type": "toggle",
        "messageKey": "tea3Enabled",
        "label": "Activer",
        "defaultValue": true
      },
      {
        "type": "input",
        "messageKey": "tea3Name",
        "label": "Nom",
        "defaultValue": "Tisane"
      },
      {
        "type": "input",
        "messageKey": "tea3Content",
        "label": "Préparation",
        "defaultValue": "1 c. à s., 100 °C"
      },
      {
        "type": "slider",
        "messageKey": "tea3Min",
        "label": "Début (min)",
        "min": 1,
        "max": 15,
        "step": 1,
        "defaultValue": 5
      },
      {
        "type": "slider",
        "messageKey": "tea3Max",
        "label": "Fin (min)",
        "min": 1,
        "max": 20,
        "step": 1,
        "defaultValue": 7
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Enregistrer"
  }
];
