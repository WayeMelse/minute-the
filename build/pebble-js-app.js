/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	/**
	 * Copyright 2024 Google LLC
	 *
	 * Licensed under the Apache License, Version 2.0 (the "License");
	 * you may not use this file except in compliance with the License.
	 * You may obtain a copy of the License at
	 *
	 *     http://www.apache.org/licenses/LICENSE-2.0
	 *
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS,
	 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	 * See the License for the specific language governing permissions and
	 * limitations under the License.
	 */
	
	(function(p) {
	  if (!p === undefined) {
	    console.error('Pebble object not found!?');
	    return;
	  }
	
	  // Aliases:
	  p.on = p.addEventListener;
	  p.off = p.removeEventListener;
	
	  // For Android (WebView-based) pkjs, print stacktrace for uncaught errors:
	  if (typeof window !== 'undefined' && window.addEventListener) {
	    window.addEventListener('error', function(event) {
	      if (event.error && event.error.stack) {
	        console.error('' + event.error + '\n' + event.error.stack);
	      }
	    });
	  }
	
	})(Pebble);


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	var msg_queue = [];
	
	function msg_ack(e) {
	  msg_queue.shift();
	  sendMessages();
	}
	
	function msg_nak(e) {
	//  console.log("nak, resending");
	  sendMessages();
	}
	
	function sendMessages() {
	  if(msg_queue.length > 0) {
	    Pebble.sendAppMessage(msg_queue[0], msg_ack, msg_nak);
	  }
	}
	
	Pebble.addEventListener("ready", function(e) {
	  var config = localStorage.getItem("config");
	  // "CANCELLED" is to work around an old bug that stored that string when the browser was closed using the back button on Android.
	  if(!config || config=="CANCELLED") {
	    var teas = [
	      [false, "White", "1 tbsp, 175-185F", 4, 12],
	      [false, "Chinese Green", "1 tsp, 185F", 12, 12],
	      [true, "Japanese Green", "1 tsp, 180F", 8, 12],
	      [true, "Black", "1 tsp, 206F", 12, 20],
	      [true, "Darjeeling", "1 tsp, 185F", 12, 12],
	      [true, "Oolong", "1 tsp, 185-206F", 12, 20],
	      [true, "Herbal Infusion", "1 tbsp, 206F", 20, 28]
	    ];
	    localStorage.setItem("config", JSON.stringify(teas));
	   }
	});
	
	Pebble.addEventListener("appmessage", function(e) {
	  updateMenu(localStorage.getItem("config"));
	});
	
	function updateMenu(conf) {
	  var teas = JSON.parse(conf);
	  var count = 0;
	  for(var i=0; i<teas.length; i++) {
	    if(teas[i][0]) {
	      count++;
	    }
	  }
	  var idx = 0;
	  for(i=0; i<teas.length; i++) {
	    if(teas[i][0]) {
	      var msg = {
	        "0": "u",
	        "1": idx,
	        "2": count,
	        "3": teas[i][1],
	        "4": teas[i][2],
	        "5": +teas[i][3],
	        "6": +teas[i][4]
	      };
	      msg_queue.push(msg);
	      idx++;
	    }
	  }
	  sendMessages();
	}
	
	Pebble.addEventListener("showConfiguration", function(e) {
	  var loc = 'http://teatime.blighty.com/teatime.html#' +
	    encodeURIComponent(localStorage.getItem("config"));
	//  console.log("redirecting to " + loc);
	  Pebble.openURL(loc);
	});
	
	Pebble.addEventListener("webviewclosed", function(e) {
	  if(e.response && e.response.length) {
	    var config = decodeURIComponent(e.response);
	//    console.log("New config = " + config);
	    if(config != "CANCELLED") {
	      localStorage.setItem("config", config);
	      updateMenu(config);
	    }
	  }
	});


/***/ })
/******/ ]);
//# sourceMappingURL=pebble-js-app.js.map