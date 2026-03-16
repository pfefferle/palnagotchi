#include "webui.h"
#include "storage.h"
#include "mood.h"
#include "config.h"
#include <WiFi.h>

static WiFiServer server(80);

void initWebUi() {
  IPAddress ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gateway, subnet);
  server.begin();
}

static String htmlEscape(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

static String rssiBar(signed int rssi) {
  if (rssi >= -67) return "||||";
  if (rssi >= -70) return "|||";
  if (rssi >= -80) return "||";
  if (rssi > -1000) return "|";
  return "";
}

void webUiTick() {
  WiFiClient client = server.accept();
  if (!client) return;

  // Wait for request data (up to 50ms)
  uint32_t start = millis();
  while (client.connected() && !client.available()) {
    if (millis() - start > 50) break;
  }

  // Read and discard the request
  while (client.available()) client.read();

  // Send response
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"));
  client.print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"));
  client.print(F("<meta http-equiv='refresh' content='5'>"));
  client.print(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  client.print(F("<title>Palnagotchi</title>"));
  client.print(F("<style>"));
  client.print(F("body{background:#000;color:#0f0;font-family:monospace;margin:20px;text-align:center}"));
  client.print(F(".face{font-size:3em;margin:20px 0}"));
  client.print(F(".phrase{font-size:1.2em;color:#0a0;margin-bottom:20px}"));
  client.print(F("table{margin:20px auto;border-collapse:collapse;text-align:left}"));
  client.print(F("th,td{padding:4px 12px;border-bottom:1px solid #030}"));
  client.print(F("th{color:#0f0}td{color:#0a0}"));
  client.print(F(".gone{color:#050}"));
  client.print(F(".log{text-align:left;max-width:600px;margin:20px auto;font-size:0.9em;color:#0a0}"));
  client.print(F("h2{color:#0f0;font-size:1em;margin-top:30px}"));
  client.print(F("</style></head><body>"));

  // Name
  client.print(F("<div style='color:#0a0'>"));
  client.print(PALNAGOTCHI_NAME);
  client.print(F("</div>"));

  // Face
  client.print(F("<div class='face'>"));
  client.print(htmlEscape(getCurrentMoodFace()));
  client.print(F("</div>"));

  // Phrase
  client.print(F("<div class='phrase'>"));
  client.print(htmlEscape(getCurrentMoodPhrase()));
  client.print(F("</div>"));

  // Peers
  uint8_t peer_count = storageGetPeerCount();
  client.print(F("<h2>Nearby ("));
  client.print(peer_count);
  client.print(F(" / "));
  client.print(storageGetTotalPeers());
  client.print(F(")</h2>"));

  if (peer_count > 0) {
    client.print(F("<table><tr><th>Name</th><th>Type</th><th>RSSI</th><th>Face</th></tr>"));
    storage_peer *peers = storageGetPeers();
    for (uint8_t i = 0; i < peer_count; i++) {
      client.print(peers[i].gone ? F("<tr class='gone'>") : F("<tr>"));
      client.print(F("<td>"));
      client.print(htmlEscape(peers[i].name));
      client.print(F("</td><td>"));
      client.print(peers[i].type);
      client.print(F("</td><td>"));
      client.print(rssiBar(peers[i].rssi));
      client.print(F(" ("));
      client.print(peers[i].rssi);
      client.print(F(")</td><td>"));
      client.print(htmlEscape(peers[i].face));
      client.print(F("</td></tr>"));
    }
    client.print(F("</table>"));
  }

  // Chat log
  uint8_t log_count = storageGetLogCount();
  client.print(F("<h2>Log</h2><div class='log'>"));
  if (log_count == 0) {
    client.print(F("<div style='color:#050'>No log entries yet.</div>"));
  }
  for (uint8_t i = 0; i < log_count; i++) {
    client.print(F("<div>"));
    client.print(htmlEscape(storageGetLogEntry(i)));
    client.print(F("</div>"));
  }
  client.print(F("</div>"));

  // Uptime
  unsigned long secs = millis() / 1000;
  char uptime[16];
  snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu", secs / 3600, (secs % 3600) / 60, secs % 60);
  client.print(F("<div style='margin-top:20px;color:#050'>UP "));
  client.print(uptime);
  if (isSdAvailable()) client.print(F(" | SD"));
  client.print(F("</div>"));

  client.print(F("</body></html>"));
  client.stop();
}
