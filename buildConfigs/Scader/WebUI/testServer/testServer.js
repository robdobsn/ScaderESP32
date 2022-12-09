'use strict';

const http = require('http');
const express = require('express');
const fs = require('fs');
const expressStaticGzip = require("express-static-gzip");
const ws = require('ws');
const parse = require('url').parse;
const cors = require('cors');

let curSettings = {
  "ScaderCommon": {
    "name": "Scader",
    "hostname": "scader",
  },
  "ScaderRelays": {
    "enable": false,
    "maxElems": 24,
    "elems": [ 
      {"idx":0,"name":"Kitchen Fridge Cupboard"},
      {"idx":1,"name":"Snug Floor Inset LEDs"},
      {"idx":2,"name":"Kitchen Floating Unit"},
      {"idx":3,"name":"Conservatory Wall Spots"},
      {"idx":4,"name":"Conservatory Up-Down"},
      {"idx":5,"name":"Lobby Ceiling Square"},
      {"idx":6,"name":"Conservatory Ceiling"},
      {"idx":7,"name":"Cellar Rack Room"},
      {"idx":8,"name":"Cellar Lobby"},
      {"idx":9,"name":"Cellar Under Kitchen"},
      {"idx":10,"name":"Cellar Low Ceiling"},
      {"idx":11,"name":"Cellar Stairs Low"},
      {"idx":12,"name":"Cellar Stair Door"},
      {"idx":13,"name":"Hall Sconce Above Cloaks Door"},
      {"idx":14,"name":"Cellar Main Room"},
      {"idx":15,"name":"Snug Book Case"}
    ]
  },
  "ScaderCat": {
    "enable":false
  },
  "ScaderOpener": {
    "enable":false
  },
  "ScaderLEDPix": { 
    "enable":false
  },
  "ScaderWaterer": {
    "enable":false,
  },
  "ScaderShades": {
    "enable": false,
    "maxElems": 5,
    "name": "Office Shades",
    "elems": [
        { "name": "Shade 1", "busy": false},
        { "name": "Shade 2", "busy": false},
        { "name": "Shade 3", "busy": false},
        { "name": "Shade 4", "busy": false},
        { "name": "Shade 5", "busy": false}
    ]
  },
  "ScaderDoors": {
    "enable": false,
    "name": "Door Control",
    "doors": [
      { "name": "Door 1" },
      { "name": "Door 2" }
    ]
  }
};

let publishesPending = [];

const states = {
}

const updateStatesFromConfig = () => {
  for (const key in curSettings) {
    if (key == 'ScaderRelays') {
      states[key] = {elems:[]};
      for (const elem of curSettings[key].elems) {
        states[key].elems.push({name:elem.name, state:0});
      }
    } else if (key == 'ScaderShades') {
      states[key] = {elems:[]};
      for (const elem of curSettings[key].elems) {
        states[key].elems.push({name:elem.name, state:0});
      }
    } else if (key == 'ScaderDoors') {
      states[key] = {doors:[]};
      for (const door of curSettings[key].doors) {
        states[key].doors.push({name:door.name, state:0});
      }
    } else {
      states[key] = {};
    }
  }
}

updateStatesFromConfig();

run().catch(err => console.log(err));

async function run() {

  const app = express();
  const server = http.createServer(app);
  const wss = new ws.Server({ server });

  // // Websockets
  // const wsServer = new ws.Server({ noServer: true });
  // wsServer.on('connection', socket => {
  //   socket.on('message', message => console.log(message));
  // });

  app.use(express.json());
  app.use(cors({origin: '*'}));

  app.get('/api/getsettings/:settings?', async function (req, res) {
    res.json({"req":"getsettings/nv","sysType":"Scader","nv": curSettings,"rslt":"ok"});
  });

  app.post('/api/postsettings', async function (req, res) {
    console.log("postsettings", req.body)
    if ("body" in req) {
      curSettings = req.body;
      console.log(`Settings updated: ${JSON.stringify(curSettings)}`);
    }
    res.json({"rslt":"ok"});
    updateStatesFromConfig();
  });

  app.get('/api/reset', async function(req, res) {
    console.log("Reset ESP32");
    res.json({"rslt":"ok"})
  });

  app.get('/api/relay/:relaynum/:mode', async function(req, res) {
    console.log('Relay control', req.params);
    res.json({"rslt":"ok"})
    if (req.params.relaynum && req.params.relaynum >= 0 && req.params.relaynum < curSettings.ScaderRelays.elems.length) {
      if (req.params.mode) {
        states.ScaderRelays.elems[req.params.relaynum].state = req.params.mode == 'on' ? 1 : 0;
        publishesPending.push("ScaderRelays");
      } else {
        console.log(`Invalid mode: ${req.params.mode}`);
      }
    } else {
      console.log(`Invalid relay number ${req.params.relaynum}`);
    }
  });

  app.get('/api/cat/squirt/:operation/:duration?', async function(req, res) {
    console.log('Cat squirt', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/cat/light/:operation/:duration?', async function(req, res) {
    console.log('Cat light', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/shade/:shadeNum/:direction/:duration', async function(req, res) {
    console.log('Shade', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/shade/:shadeNum/:action', async function(req, res) {
    console.log('Shade', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/door/:doorIds/:action/:userNum/:userPin', async function(req, res) {
    console.log('Door', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/opener/mode/:mode', async function(req, res) {
    console.log('Conservatory Opener Mode', req.params);
    curSettings['ScaderOpener']['mode'] = req.params.mode;
    res.json({"rslt":"ok"})
  });

  app.get('/api/opener/open', async function(req, res) {
    console.log('Conservatory Open Door', req.params);
    res.json({"rslt":"ok"})
  });

  app.get('/api/opener/close', async function(req, res) {
    console.log('Conservatory Close Door', req.params);
    res.json({"rslt":"ok"})
  });

  // app.get('/events', async function(req, res) {
  //   console.log('Got /events');
  //   res.set({
  //     'Cache-Control': 'no-cache',
  //     'Content-Type': 'text/event-stream',
  //     'Connection': 'keep-alive'
  //   });
  //   res.flushHeaders();

  //   // Tell the client to retry every 10 seconds if connectivity is lost
  //   res.write('retry: 10000\n\n');
  //   let count = 0;

  //   while (true) {
  //     await new Promise(resolve => setTimeout(resolve, 1000));

  //     console.log('Emit', ++count);
  //     // Emit an SSE that contains the current 'count' as a string
  //     res.write(`data: ${count}\n\n`);
  //   }
  // });

  // app.use("/", expressStaticGzip("./origSite/"));
  app.use("/", expressStaticGzip("../build/"));
  // app.use("/", express.static('./buildConfigs/Scader/FSImage/'))
  // const index = fs.readFileSync('./buildConfigs/Common/Scader/FSImage/index.html', 'utf8');
  // app.get('/', (req, res) => res.send(index));

  // const server = await app.listen(3123);
  server.listen(3123, () => {
    console.log('Listening on %d', server.address().port);
  });

  wss.on('connection', (webSocketClient) => {
    console.log('Got connection');
    webSocketClient.on('message', (message) => {
      console.log('received: %s', message);
    });

    const intervalTimer = setInterval(() => {
      // Go through pending publishes and send them
      for (const key of publishesPending) {
        console.log(`Publishing to ${wss.clients} clients ${JSON.stringify(states[key])}`);
        webSocketClient.send(JSON.stringify(states[key]));
      }
      publishesPending = [];
    }, 100);

    webSocketClient.on('close', () => {
      console.log('disconnected');
      clearInterval(intervalTimer);
    });
  });

  // server.on('upgrade', (request, socket, head) => {
  //   const { pathname } = parse(request.url);

  //   console.log(`Upgrade: ${pathname}`);

  //   if (pathname === '/scader') {
  //     wss.handleUpgrade(request, socket, head, function done(ws) {
  //       wss.emit('connection', ws, request);

  //       ws.on('message', function incoming(message) {
  //         console.log('received: %s', message);
  //       });

  //       ws.on('open', function connection(ws) {
  //         console.log('New connection');
  //       });

  //       const intervalTimer = setInterval(() => {
  //         // Go through pending publishes and send them
  //         for (const key of publishesPending) {
  //           console.log(`Publishing ${JSON.stringify(states[key])}`);
  //           wss.emit(JSON.stringify(states[key]), ws, request);
  //         }
  //         publishesPending = [];
  //       }, 1000);

  //       ws.on('close', function close() {
  //         clearInterval(intervalTimer);
  //       });

  //     });
  //   } else {
  //     socket.destroy();
  //   }
  // });  
}
