var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var defaults = require('./defaults');
var customClay = require('./custom-clay');
var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

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

function defaultTeaColor(name) {
  var text = String(name || '').toLowerCase();
  if(text.indexOf('blanc') >= 0 || text.indexOf('white') >= 0) return 0;
  if(text.indexOf('vert') >= 0 || text.indexOf('green') >= 0) return 1;
  if(text.indexOf('oolong') >= 0) return 3;
  if(text.indexOf('tisane') >= 0 || text.indexOf('herbal') >= 0) return 4;
  return 2;
}

function teaColor(tea) {
  var color = parseInt(tea[5], 10);
  return color >= 0 && color <= 4 ? color : defaultTeaColor(tea[1]);
}

function defaultTeas() {
  var teas = [];
  defaults.forEach(function(t) {
    if (t.enabled) {
      teas.push([true, t.name, t.content, t.min * 4, t.max * 4, defaultTeaColor(t.name)]);
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

  msg_queue.push({
    0: 't',
    8: localStorage.getItem('themeLight') === '1' ? 1 : 0
  });
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
        6: teas[i][4],
        7: teaColor(teas[i])
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
  var themeValue = val(settings, 'themeLight');
  if (themeValue !== undefined) {
    var isLight = themeValue === true || themeValue === 1 ||
      themeValue === '1' || themeValue === 'true';
    localStorage.setItem('themeLight', isLight ? '1' : '0');
  }
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
        maxMinutes * 4,
        parseInt(val(settings, 'tea' + i + 'Color'), 10)
      ]);
    }
  }

  if (teas.length === 0) {
    teas = defaultTeas();
  }

  localStorage.setItem('config', JSON.stringify(teas));
  updateMenu(JSON.stringify(teas));
});
