/*  WIFI-PPM
 *   
 *  The program makes a PPM Signal for controlling a RC device at PIN 5. After connectig to the Access Point
 *  you can open the Website for controlling by typing the IP Adress in the Web Browser. The website supports 
 *  touchscreen or joystick input with four analog axis and four digital buttons. (The joystik works only with 
 *  firefox at the moment)  
 *  OTA update is also supported. Just open IP-Address/update and upload the new .bin file.
 *  I use the device for controlling my micro indoor quadcopter.
 *  
 *  code used for the program:
 *  - Arduino example scatches
 *  - Arduino websockets library from https://github.com/Links2004/arduinoWebSockets
 *  - Arduino ESP8266 librarys from http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  - PPM generator originally written by David Hasko from https://quadmeup.com/generate-ppm-signal-with-arduino/
 *  - some examples from https://www.w3schools.com/
 *  
 *  created 10 Dez. 2017
 *  by Andi Mösenlechner
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Hash.h>

/* Set these to your desired credentials. */
const char *ssid = "WifiPPM";      //Name of the access point
const char *password = ""; //Password for the access point

volatile unsigned long next;

#define CPU_MHZ 80
#define CHANNEL_NUMBER 6           //set the number of chanels
#define CHANNEL_DEFAULT_VALUE 1100 //set the default servo value
#define FRAME_LENGTH 20000         //set the PPM frame length in microseconds (1ms = 1000µs)
#define PULSE_LENGTH 300           //set the pulse length
#define onState 0                  //set polarity of the pulses: 1 is positive, 0 is negative
#define sigPin 0                   //set PPM signal output pin on the arduino
#define DEBUGPIN 2                 //used as trigger reference for the oscilloscope

int ppm[CHANNEL_NUMBER];                                                                                                                                                //just used for captive portal

DNSServer dnsServer; 
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void inline ppmISR(void)
{ //PPM Interrupt
    static boolean state = true;

    if (state)
    { //start pulse
        digitalWrite(sigPin, onState);
        next = next + (PULSE_LENGTH * CPU_MHZ);
        state = false;
    }
    else
    { //end pulse and calculate when to start the next pulse
        static byte cur_chan_numb;
        static unsigned int calc_rest;

        digitalWrite(sigPin, !onState);
        state = true;

        if (cur_chan_numb >= CHANNEL_NUMBER)
        {
            cur_chan_numb = 0;
            calc_rest = calc_rest + PULSE_LENGTH; //
            next = next + ((FRAME_LENGTH - calc_rest) * CPU_MHZ);
            calc_rest = 0;
            digitalWrite(DEBUGPIN, !digitalRead(DEBUGPIN)); //Toggle debug pin after each complete frame as trigger for the oscilloscope
        }
        else
        {
            next = next + ((ppm[cur_chan_numb] - PULSE_LENGTH) * CPU_MHZ);
            calc_rest = calc_rest + ppm[cur_chan_numb];
            cur_chan_numb++;
        }
    }
    timer0_write(next);
}

void handleRoot()
{
    String website = "";
    website += " <!DOCTYPE html>\n";
    website += "    <head>\n";
    website += "    <meta charset=\"utf-8\"> <title>Wifi PPM</title>\n";
    website += "    <meta name=\"viewport\" content=\"width=device-width, user-scalable=no\">\n";

    website += "    <style>\n";

    website += "    .switch {\n";
    website += "      position: relative;\n";
    website += "      display: block;\n";
    website += "      margin-left: auto;\n";
    website += "      margin-right: auto;\n";
    website += "      width: 34px;\n";
    website += "      height: 34px;\n";
    website += "    }\n";

    website += "    .switch input {display:none;}\n";

    website += "    .slider {\n";
    website += "      position: absolute;\n";
    website += "      cursor: pointer;\n";
    website += "      border-radius: 34px;\n";
    website += "      top: 0;\n";
    website += "      left: 0;\n";
    website += "      right: 0;\n";
    website += "      bottom: 0;\n";
    website += "      background-color: #F00;\n";
    website += "    }\n";

    website += "    input:checked + .slider {\n";
    website += "      background-color: #0F0;\n";
    website += "    }\n";

    website += "    </style>\n";
    website += "    </head>\n";
    website += "    <body onload=\"start()\">\n";
    website += "    <br>\n";
    website += "    <label class=\"switch\">\n";
    website += "      <input type=\"checkbox\" id=\"Button0\" onchange=\"Button1Change(this)\">\n";
    website += "      <span class=\"slider\"></span>\n";
    website += "    </label>\n";
    website += "    <br>\n";
    website += "    <label class=\"switch\">\n";
    website += "      <input type=\"checkbox\" id=\"Button1\" onchange=\"Button2Change(this)\">\n";
    website += "      <span class=\"slider\"></span>\n";
    website += "    </label>\n";

    website += "    <div id=\"invert1div\" style=\"display: none; position: absolute; left: 20%; top: 1%;\">\n";
    website += "      Invert Y Axis<input type=\"checkbox\" id=\"invert1\">\n";
    website += "    </div>\n";
    website += "    <div id=\"invert2div\" style=\"display: none; position: absolute; left: 70%; top: 1%;\">\n";
    website += "      Invert Y Axis<input type=\"checkbox\" id=\"invert2\">\n";
    website += "    </div>\n";

    website += "    <div id=\"trimdiv\" style=\"display:none;\">\n"; //remove "display:none;" if you want trim sliders in touchscreen mode
    website += "      <input type=\"range\" min=\"-100\" max=\"100\" value=\"0\" class=\"slider\" id=\"trim1x\" style=\"position: absolute; left: 5%; top: 95%; width: 38%;\">\n";
    website += "      <input type=\"range\" min=\"-100\" max=\"100\" value=\"0\" class=\"slider\" id=\"trim2x\" style=\"position: absolute; left: 55%; top: 95%; width: 38%;\">\n";
    website += "      <input type=\"range\" min=\"-100\" max=\"100\" value=\"0\" class=\"slider\" id=\"trim2y\" orient=\"vertical\" style=\"position: absolute; left: 98%; top: 10%; height: 78%;-webkit-appearance: slider-vertical;\">\n";
    website += "      <input type=\"range\" min=\"-100\" max=\"100\" value=\"0\" class=\"slider\" id=\"trim1y\" orient=\"vertical\" style=\"position: absolute; left: 1%; top: 10%; height: 78%;-webkit-appearance: slider-vertical;\">\n";
    website += "    </div>\n";
    website += "    <div id=textdiv style=\"position:absolute;top:0px;left:0px;\"></div>\n";
    website += "    <canvas id=\"Canvas_left\" style=\"border:1px solid #d3d3d3;position:absolute; top:10%; left:5%; z-index:0\">\n";
    website += "    Your browser does not support the HTML5 canvas tag.</canvas>\n";
    website += "    <canvas id=\"Canvas_right\" style=\"border:1px solid #d3d3d3;position:absolute; top:10%; left:55%; z-index:0\">\n";
    website += "    Your browser does not support the HTML5 canvas tag.</canvas>\n";
    website += "    <canvas id=\"Canvas_stickl\" style=\"position:absolute; top:10%; left:5%; z-index:1\">\n";
    website += "    Your browser does not support the HTML5 canvas tag.</canvas>\n";
    website += "    <canvas id=\"Canvas_stickr\" style=\"position:absolute; top:10%; left:55%; z-index:1\">\n";
    website += "    Your browser does not support the HTML5 canvas tag.</canvas>\n";

    website += "    <script>\n";
    website += "    var touches = [];\n";
    website += "    var w = 0;\n";
    website += "    var wsconnect = 0;\n";
    website += "    var h = 0;\n";
    website += "    var end=0;\n";
    website += "    var ctx_left;\n";
    website += "    var ctx_right;\n";
    website += "    var ctx_stickl;\n";
    website += "    var ctx_stickr;\n";
    website += "    var gamepads = {};\n";
    website += "    var buttons=[0,0];\n";
    website += "    var ppm=[1100,1100,1100,1100,1100,1100];\n";
    website += "    var oldppm=[0,0,0,0,0,0];\n";

    website += "    var connection = new WebSocket('ws://' + window.location.host + ':81', ['arduino'])\n";

    website += "    connection.onopen = function () {         //open\n";
    website += "      console.log(\"Websocket Open\");\n";
    website += "      wsconnect=1;\n";
    website += "      draw_stick(ctx_stickl,ctx_stickl.canvas.width/2,ctx_stickl.canvas.height,0,1);\n";
    website += "      draw_stick(ctx_stickr,ctx_stickr.canvas.width/2,ctx_stickr.canvas.height/2,2,3);\n";
    website += "    };\n";

    website += "    connection.onerror = function (error) {   //error\n";
    website += "      console.log('WebSocket Error ' + error);\n";
    website += "      wsconnect=0;\n";
    website += "      draw_stick(ctx_stickl,ctx_stickl.canvas.width/2,ctx_stickl.canvas.height,0,1);\n";
    website += "      draw_stick(ctx_stickr,ctx_stickr.canvas.width/2,ctx_stickr.canvas.height/2,2,3);\n";
    website += "    };\n";

    website += "    connection.onmessage = function (e) {\n";
    website += "       console.log(\"indata: \" + JSON.stringify(e));\n";
    website += "    }\n";

    website += "    connection.onclose = function (e)\n";
    website += "    {\n";
    website += "        console.log(\"Websocket close\");\n";
    website += "      wsconnect=0;\n";
    website += "      draw_stick(ctx_stickl,ctx_stickl.canvas.width/2,ctx_stickl.canvas.height,0,1);\n";
    website += "      draw_stick(ctx_stickr,ctx_stickr.canvas.width/2,ctx_stickr.canvas.height/2,2,3);\n";
    website += "    }\n";

    website += "    function start()\n";
    website += "    {\n";
    website += "      var c_left = document.getElementById(\"Canvas_left\");\n";
    website += "      ctx_left = c_left.getContext(\"2d\");\n";

    website += "      var c_right = document.getElementById(\"Canvas_right\");\n";
    website += "      ctx_right = c_right.getContext(\"2d\");\n";

    website += "      var c_stickl = document.getElementById(\"Canvas_stickl\");\n";
    website += "      ctx_stickl = c_stickl.getContext(\"2d\");\n";

    website += "      var c_stickr = document.getElementById(\"Canvas_stickr\");\n";
    website += "      ctx_stickr = c_stickr.getContext(\"2d\");\n";

    website += "      update();\n";

    website += "      draw_background(ctx_left);\n";
    website += "      draw_background(ctx_right);\n";
    website += "      draw_stick(ctx_stickl,c_stickl.width/2,c_stickl.height,0,1);\n";
    website += "      draw_stick(ctx_stickr,c_stickr.width/2,c_stickr.height/2,2,3);\n";

    website += "      window.addEventListener(\"optimizedResize\", function() {\n";
    website += "        resize();\n";
    website += "      });  \n";
    website += "      window.addEventListener(\"orientationchange\", function() {\n";
    website += "        window.setTimeout(resize, 300)\n";
    website += "      });    \n";
    website += "      c_stickl.addEventListener('touchend', function() {\n";
    website += "        console.log( \"endl\");\n";
    website += "      });\n";
    website += "      c_stickl.addEventListener('touchmove', function(event) {\n";
    website += "        event.preventDefault();\n";
    website += "        touches = event.touches;\n";
    website += "      });\n";
    website += "      c_stickl.addEventListener('touchstart', function(event) {\n";
    website += "        console.log('startl');\n";
    website += "      });\n";
    website += "      c_stickr.addEventListener('touchend', function() {\n";
    website += "        console.log(\"endr\");\n";
    website += "        end=1;\n";
    website += "        draw_stick(ctx_stickr,c_stickr.width/2,c_stickr.height/2,2,3);\n";
    website += "      });\n";
    website += "      c_stickr.addEventListener('touchmove', function(event) {\n";
    website += "        event.preventDefault();\n";
    website += "        touches = event.touches;\n";
    website += "      });\n";
    website += "      c_stickr.addEventListener('touchstart', function(event) {\n";
    website += "        console.log('startr');\n";
    website += "        end=0;\n";
    website += "      });\n";
    website += "      window.addEventListener(\"gamepadconnected\", function(e) { gamepadHandler(e, true); }, false);\n";
    website += "      window.addEventListener(\"gamepaddisconnected\", function(e) { gamepadHandler(e, false); }, false);\n";
    website += "    };\n";

    website += "    function Button1Change(checkbox)\n";
    website += "    {\n";
    website += "      if(checkbox.checked==true)\n";
    website += "        ppm[4]=1900;\n";
    website += "      else\n";
    website += "        ppm[4]=1100;\n";
    website += "      console.log(\"Button1: \" + ppm[4]);\n";
    website += "    }\n";
    website += "    function Button2Change(checkbox)\n";
    website += "    {\n";
    website += "      if(checkbox.checked==true)\n";
    website += "        ppm[5]=1900;\n";
    website += "      else\n";
    website += "        ppm[5]=1100;\n";
    website += "      console.log(\"Button2: \" + ppm[5]);\n";
    website += "    }\n";

    website += "    function resize()\n";
    website += "    {\n";
    website += "      ctx_left.canvas.height=window.innerHeight-(window.innerHeight/10*2);\n";
    website += "      ctx_left.canvas.width=(window.innerWidth-(window.innerWidth/10*2))/2;\n";

    website += "      ctx_right.canvas.height=window.innerHeight-(window.innerHeight/10*2);\n";
    website += "      ctx_right.canvas.width=(window.innerWidth-(window.innerWidth/10*2))/2;\n";

    website += "      ctx_stickl.canvas.height=ctx_left.canvas.height;\n";
    website += "      ctx_stickl.canvas.width=ctx_left.canvas.width;\n";

    website += "      ctx_stickr.canvas.height=ctx_right.canvas.height;\n";
    website += "      ctx_stickr.canvas.width=ctx_right.canvas.width;\n";

    website += "      document.getElementById(\"trim1x\").min=-(ctx_stickl.canvas.width/4);\n";
    website += "      document.getElementById(\"trim1x\").max=(ctx_stickl.canvas.width/4);\n";
    website += "      document.getElementById(\"trim2x\").min=-(ctx_stickr.canvas.width/4);\n";
    website += "      document.getElementById(\"trim2x\").max=(ctx_stickr.canvas.width/4);\n";
    website += "      document.getElementById(\"trim1y\").min=-(ctx_stickl.canvas.width/4);\n";
    website += "      document.getElementById(\"trim1y\").max=(ctx_stickl.canvas.width/4);\n";
    website += "      document.getElementById(\"trim2y\").min=-(ctx_stickr.canvas.width/4);\n";
    website += "      document.getElementById(\"trim2y\").max=(ctx_stickr.canvas.width/4);\n";

    website += "      draw_background(ctx_left);\n";
    website += "      draw_background(ctx_right);\n";
    website += "      draw_stick(ctx_stickl,ctx_stickl.canvas.width/2,ctx_stickl.canvas.height);\n";
    website += "      draw_stick(ctx_stickr,ctx_stickr.canvas.width/2,ctx_stickr.canvas.height/2);\n";

    website += "    }\n";

    website += "    function draw_stick(context,x,y,ppm0,ppm1)\n";
    website += "    {\n";
    website += "      context.clearRect(0, 0, context.canvas.width, context.canvas.height);\n";

    website += "          context.beginPath();\n";
    website += "            context.arc(x,y,window.innerWidth/100*2,0,2*Math.PI);\n";
    website += "            if(wsconnect)\n";
    website += "              context.fillStyle = 'green';\n";
    website += "            else\n";
    website += "              context.fillStyle = 'red';\n";
    website += "            context.fill();\n";
    website += "            context.lineWidth = 5;\n";
    website += "            context.strokeStyle = '#003300';\n";
    website += "            context.stroke();\n";

    website += "      ppm[ppm0] = parseInt(1000+((1000/context.canvas.width)*x))\n";
    website += "      ppm[ppm1] = parseInt(2000-((1000/context.canvas.height)*y))\n";
    website += "    }\n";

    website += "    function draw_background(ctx)\n";
    website += "    {\n";
    website += "             ctx.beginPath();\n";
    website += "             for(var i=0;i<ctx.canvas.width/2;i+=ctx.canvas.width/20)\n";
    website += "             {\n";
    website += "                     ctx.moveTo(ctx.canvas.width/2+i,ctx.canvas.height/2);\n";
    website += "                     ctx.arc(ctx.canvas.width/2,ctx.canvas.height/2,i,0,2*Math.PI);\n";
    website += "             }\n";
    website += "             ctx.moveTo(0,ctx.canvas.height/2);\n";
    website += "             ctx.lineTo(ctx.canvas.width,ctx.canvas.height/2);\n";
    website += "             ctx.moveTo(ctx.canvas.width/2,0);\n";
    website += "             ctx.lineTo(ctx.canvas.width/2,ctx.canvas.height);\n";
    website += "             ctx.stroke();\n";
    website += "    };\n";

    website += "    function gamepadHandler(event, connecting) {\n";
    website += "      var gamepad = event.gamepad;\n";

    website += "      if (connecting) {\n";
    website += "        gamepads[gamepad.index] = gamepad;\n";
    website += "        console.log(\"Joystick connected \" + gamepad.index);\n";
    website += "        document.getElementById(\"invert1div\").style.display=\"block\";\n";
    website += "        document.getElementById(\"invert2div\").style.display=\"block\";\n";
    website += "        document.getElementById(\"trimdiv\").style.display=\"block\";\n";
    website += "        update();\n";
    website += "      } else {\n";
    website += "        console.log(\"Joystick disconnect\");\n";
    website += "        delete gamepads[gamepad.index];\n";
    website += "        document.getElementById(\"invert1div\").style.display=\"none\";\n";
    website += "        document.getElementById(\"invert2div\").style.display=\"none\";\n";
    website += "        document.getElementById(\"trimdiv\").style.display=\"none\";\n";
    website += "      }\n";
    website += "    }\n";

    website += "    function checkButton(gamepad,index){\n";
    website += "        var button = gamepad.buttons[index]\n";
    website += "        if(button.value && !buttons[index])\n";
    website += "        {\n";
    website += "          buttons[index]=1;\n";
    website += "          console.log(\"Button\" + index);\n";
    website += "          if(!document.getElementById(\"Button\"+index).checked)\n";
    website += "          {\n";
    website += "            document.getElementById(\"Button\"+index).checked = true;\n";
    website += "            ppm[4+index]=1800;\n";
    website += "          }\n";
    website += "          else\n";
    website += "          {\n";
    website += "            document.getElementById(\"Button\"+index).checked = false;\n";
    website += "            ppm[4+index]=1100;\n";
    website += "          }\n";
    website += "        }\n";
    website += "        if(!button.value)\n";
    website += "          buttons[index]=0;   \n";
    website += "      }\n";

    website += "    function update() {\n";
    website += "      var nw = window.innerWidth;\n";
    website += "      var nh = window.innerHeight;\n";
    website += "      if ((w != nw) || (h != nh)) {\n";
    website += "        w = nw;\n";
    website += "        h = nh;\n";
    website += "        resize();\n";
    website += "      }\n";
    website += "      for(var i=0;i<6;i++){\n";
    website += "        if(ppm[i]!=oldppm[i])\n";
    website += "        {\n";
    website += "          oldppm[i]=ppm[i];\n";
    website += "          var sendframe=new Uint8Array(3);\n";
    website += "          sendframe[0]=i;\n";
    website += "          sendframe[1]=ppm[i]>>8;\n";
    website += "          sendframe[2]=ppm[i];\n";
    website += "          if(wsconnect)\n";
    website += "            connection.send(sendframe);\n";
    website += "        }\n";
    website += "      }\n";
    website += "      const padKeys = Object.keys(gamepads)\n";
    website += "      if(padKeys.length>0) {\n";
    website += "        padKeys.forEach(padIndex => {\n";
	website += "          const gamepad = window.navigator.getGamepads()[padIndex]\n";
    website += "          var padx0=gamepad.axes[0]*ctx_stickl.canvas.width/2;\n";
    website += "          var pady0=gamepad.axes[1]*ctx_stickl.canvas.height/2;\n";
    website += "          var padx1=gamepad.axes[5]*ctx_stickr.canvas.width/2;\n";    
    website += "          var pady1=gamepad.axes[2]*ctx_stickr.canvas.height/2;\n";
    website += "          if(document.getElementById(\"invert1\").checked)\n";
    website += "            pady0=-pady0;\n";
    website += "          if(document.getElementById(\"invert2\").checked)\n";
    website += "            pady1=-pady1;\n";
    website += "          draw_stick(ctx_stickl,parseInt(document.getElementById(\"trim1x\").value)+(ctx_stickl.canvas.width/2)+padx0,(ctx_stickl.canvas.height/2)+pady0-parseInt(document.getElementById(\"trim1y\").value),0,1);\n";
    website += "          draw_stick(ctx_stickr,parseInt(document.getElementById(\"trim2x\").value)+(ctx_stickr.canvas.width/2)+padx1,(ctx_stickr.canvas.height/2)+pady1-parseInt(document.getElementById(\"trim2y\").value),2,3);\n";
	website += "          checkButton(gamepad,0)\n";
	website += "	      checkButton(gamepad,1)\n";
	website += "        })\n";
    website += "        window.requestAnimationFrame(update);\n";
    website += "      }\n";
    website += "      else\n";
    website += "      {\n";
    website += "        var i, len = touches.length;\n";
    website += "        var left=0;\n";
    website += "        var right=0;\n";
    website += "        for (i=0; i<len; i++) {\n";
    website += "          var touch = touches[i];\n";
    website += "          var px = touch.pageX-touch.target.offsetLeft;\n";
    website += "          var py = touch.pageY-touch.target.offsetTop;\n";
    website += "          console.log(touch.target.id);\n";
    website += "          if(touch.target.id==\"Canvas_stickl\" && !left)\n";
    website += "          {\n";
    website += "            if(px>ctx_stickl.canvas.width)\n";
    website += "              px=ctx_stickl.canvas.width;\n";
    website += "            if(py<0)\n";
    website += "              py=0;\n";
    website += "            if(px<0)\n";
    website += "              px=0;\n";
    website += "            if(py>ctx_stickl.canvas.height)\n";
    website += "              py=ctx_stickl.canvas.height;\n";
    website += "            draw_stick(ctx_stickl,px,py,0,1);\n";
    website += "            left=1;\n";
    website += "          }\n";
    website += "          if(touch.target.id==\"Canvas_stickr\" && !right && !end)\n";
    website += "          {\n";
    website += "            if(px>ctx_stickr.canvas.width)\n";
    website += "              px=ctx_stickr.canvas.width;\n";
    website += "            if(py<0)\n";
    website += "              py=0;\n";
    website += "            if(px<0)\n";
    website += "              px=0;\n";
    website += "            if(py>ctx_stickr.canvas.height)\n";
    website += "              py=ctx_stickr.canvas.height;\n";
    website += "            draw_stick(ctx_stickr,px,py,2,3);\n";
    website += "            right=1;\n";
    website += "          }\n";
    website += "        }\n";
    website += "      }\n";
    website += "    }\n";
    website += "    </script>\n";

    website += "    </body>\n";
    website += "    </html>\n";

    server.send(200, "text/html", website);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

    //Serial.printf("event %i\n",type);
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
    }
    break;
    case WStype_TEXT:
    {
        Serial.printf("[%u] get Text: %s\n", num, payload);
    }
    break;
    case WStype_BIN:
    {
        ppm[payload[0]] = (payload[1] << 8) + payload[2]; //the ppm data is sent by the website in three bytes. channel/hbyte/lbyte
        Serial.printf("ppm[%u] : %d\n", payload[0], ppm[payload[0]]);
    }
    break;
    default:
    break;
    }
}

void setup()
{
    pinMode(sigPin, OUTPUT);
    digitalWrite(sigPin, !onState); //set the PPM signal pin to the default state
    pinMode(DEBUGPIN, OUTPUT);
    digitalWrite(DEBUGPIN, !onState); //set the DEBUG pin to the default state

    Serial.begin(115200);
    Serial.println();
    Serial.print("Configuring access point...");

    WiFi.softAP(ssid, password);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", myIP);

    server.onNotFound(handleRoot);
    server.on("/", handleRoot);

    server.begin();
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();

    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(ppmISR);
    next = ESP.getCycleCount() + 1000;
    timer0_write(next);
    for (int i = 0; i < CHANNEL_NUMBER; i++)
    {
        ppm[i] = CHANNEL_DEFAULT_VALUE;
    }
    interrupts();

    Serial.println("HTTP server started");
}

void loop()
{
    webSocket.loop();
    dnsServer.processNextRequest();
    server.handleClient();
}