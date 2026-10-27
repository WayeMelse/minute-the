var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var defaults = require('./defaults');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

function val(s, k) {
  var x = s[k];
  return (x && typeof x === 'object' && 'value' in x) ? x.value : x;
}

var msg_queue = [];

function msg_ack() {
  msg_queue.shift();
  sendMessages();
}

function msg_nak() {
  sendMessages();
}

function sendMessages() {
  if (msg_queue.length > 0) {
    Pebble.sendAppMessage(msg_queue[0], msg_ack, msg_nak);
  }
}

function defaultTeas() {
  var teas = [];
  defaults.forEach(function(t) {
    if (t.enabled) {
      teas.push([true, t.name, t.content, t.min * 4, t.max * 4]);
    }
  });
  return teas;
}

Pebble.addEventListener('ready', function() {
  var config = localStorage.getItem('config');

  if (!config || config === 'CANCELLED') {
    config = JSON.stringify(defaultTeas());
    localStorage.setItem('config', config);
  }

  updateMenu(config);
});

Pebble.addEventListener('appmessage', function() {
  updateMenu(localStorage.getItem('config'));
});

function updateMenu(config) {
  var teas = JSON.parse(config);
  var count = 0;
  var i;

  for (i = 0; i < teas.length; i++) {
    if (teas[i][0]) {
      count++;
    }
  }

  var idx = 0;
  for (i = 0; i < teas.length; i++) {
    if (teas[i][0]) {
      msg_queue.push({
        0: 'u',
        1: idx,
        2: count,
        3: teas[i][1],
        4: teas[i][2],
        5: teas[i][3],
        6: teas[i][4]
      });
      idx++;
    }
  }

  sendMessages();
}

Pebble.addEventListener('showConfiguration', function() {
  var url = clay.generateUrl();
  console.log('Config URL: ' + url);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }

  var settings = JSON.parse(decodeURIComponent(e.response));
  var teas = [];

  for (var i = 1; i <= defaults.length; i++) {
    if (val(settings, 'tea' + i + 'Enabled')) {
      var minMinutes = parseInt(val(settings, 'tea' + i + 'Min'), 10) || 3;
      var maxMinutes = parseInt(val(settings, 'tea' + i + 'Max'), 10) || 5;

      if (maxMinutes < minMinutes) {
        maxMinutes = minMinutes;
      }

      teas.push([
        true,
        val(settings, 'tea' + i + 'Name') || ('Thé ' + i),
        val(settings, 'tea' + i + 'Content') || '',
        minMinutes * 4,
        maxMinutes * 4
      ]);
    }
  }

  if (teas.length === 0) {
    teas = defaultTeas();
  }

  localStorage.setItem('config', JSON.stringify(teas));
  updateMenu(JSON.stringify(teas));
});
