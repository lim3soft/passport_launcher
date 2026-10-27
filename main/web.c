#include "manager.h"
#include "compat.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static httpd_handle_t server;
static esp_netif_t *ap;
static bool wifi_initialized, wifi_started, network_initialized;
static const char password[] = "aipassport";
static char token[33];

const char *passport_ap_password(void) { return password; }

static const char html[] =
"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Passport Launcher</title><style>body{font:16px system-ui;max-width:620px;margin:32px auto;padding:20px;"
"background:#0c151d;color:#f3f7fa}button,input{box-sizing:border-box;padding:12px;margin:6px 0;max-width:100%}button{cursor:pointer;border:0;border-radius:16px;background:#c3f77a;color:#0c151d}"
"article{background:#192a35;border-radius:18px;padding:18px;margin:12px 0}small{display:block;color:#9bb2c0}a{color:#c3f77a}label{display:block}.danger{background:#ffa0a0}nav{display:flex;gap:16px;flex-wrap:wrap}progress{width:100%;height:22px;accent-color:#c3f77a}footer{margin-top:32px;padding-top:16px;border-top:1px solid #344b59;color:#9bb2c0}</style>"
"<nav><a href=/about>About</a></nav><h1>Passport Launcher</h1><p>Your ROM library. Choose a .bin, install it here, then select it on the device to launch.</p>"
"<p>Apps share available flash, with private settings reserved for each app. Standard AI-Passport merged binaries are extracted locally; their bootloader and partition table are never installed.</p>"
"<p id=capacity></p>"
"<input type=file id=file accept='.bin'><div id=identity hidden>"
"<label>App name <input id=appname maxlength=31></label><br><label>App ID <input id=appid maxlength=63></label>"
"<small>Use a unique ID for each app. Reuse that ID for updates to keep its settings.</small></div>"
"<button id=install>Install / update</button>"
"<section id=transfer hidden><label id=progress-note role=status aria-live=polite>Waiting</label><progress id=install-progress max=100 value=0 aria-labelledby=progress-note></progress></section>"
"<p>Community ESP-IDF 5.5.x apps use saved default-NVS contexts (16 KiB). Power off/on to return to the launcher. Firmware needing extra resource/data partitions requires a compatible port. Firmware has full flash access; install trusted apps.</p>"
"<p>Updating keeps that app's saved data. Uninstalling erases the app and all its saved data after confirmation.</p>"
"<pre id=status></pre><div id=apps></div><footer>Developed by 9W3SPY | Visit <a href=https://github.com/lim3soft/passport_launcher target=_blank rel=noopener>GitHub</a></footer><script src=/bin-import.js></script><script>const token='";
static const char script[] =
"';const status=document.querySelector('#status'),apps=document.querySelector('#apps');"
"const picker=document.querySelector('#file'),identity=document.querySelector('#identity'),appname=document.querySelector('#appname'),appid=document.querySelector('#appid');"
"const progress=document.querySelector('#install-progress'),progressNote=document.querySelector('#progress-note'),transfer=document.querySelector('#transfer');"
"let inspected=null,inspectedFile=null,installing=false;"
"function stage(message,value){transfer.hidden=false;progressNote.textContent=message;if(value===null)progress.removeAttribute('value');else progress.value=value;}"
"async function reportError(e){try{await fetch('/api/import-error',{method:'POST',headers:{'X-Passport-Token':token},body:JSON.stringify({code:e.code||'invalid',bytes:e.bytes||0})})}catch(ignore){}}"
"picker.onchange=async()=>{if(installing)return;inspected=null;inspectedFile=null;identity.hidden=true;transfer.hidden=true;let f=picker.files[0];if(!f)return;"
"if(!/\\.bin$/i.test(f.name)){status.textContent='Choose a .bin firmware file.';await reportError({code:'invalid'});return;}"
"try{status.textContent='Checking firmware locally...';let info=PassportBin.inspect(await f.arrayBuffer(),f.name);"
"if(picker.files[0]!==f)return;inspected=info;inspectedFile=f;identity.hidden=false;appname.value=info.name;appid.value=info.id;appid.readOnly=info.abi===2;"
"status.textContent=(info.merged?'Full image: extracted app only. ':'App-only image. ')+info.image.length+' bytes. '+(info.abi?'Launcher SDK app.':'Community app: power-cycle to return.');}"
"catch(e){status.textContent=e.message;await reportError(e)}};"
"async function action(path,body,id){let headers={'X-Passport-Token':token};if(id)headers['X-Passport-App-ID']=id;let r=await fetch(path,{method:'POST',headers,body});status.textContent=await r.text();await list();}"
"function upload(body){return new Promise((resolve,reject)=>{const request=new XMLHttpRequest();request.open('POST','/api/install-bin');request.setRequestHeader('X-Passport-Token',token);request.timeout=330000;"
"request.upload.onprogress=e=>{if(e.lengthComputable&&e.total>0){let percent=Math.min(100,Math.round(100*e.loaded/e.total));stage('Uploading BIN: '+percent+'%',percent)}else stage('Uploading BIN...',null)};"
"request.upload.onload=()=>stage('Installing and verifying on the device. Keep power on.',null);"
"request.onload=()=>{status.textContent=request.responseText;if(request.status>=200&&request.status<300){stage('Installation complete. Select the app on your device.',100);resolve()}else{stage('Installation failed. See the error below.',0);resolve()}};"
"request.onerror=request.ontimeout=request.onabort=()=>reject(new Error('Connection interrupted. Installation was not confirmed. Retry the same BIN.'));"
"request.send(body);});}"
"document.querySelector('#install').onclick=async()=>{if(installing)return;let f=picker.files[0];if(!f)return;let body;"
"try{if(!/\\.bin$/i.test(f.name)||inspectedFile!==f||!inspected)throw new Error('Select a valid .bin and wait for its checks to finish.');body=PassportBin.pack(inspected,appname.value.trim(),appid.value.trim());}"
"catch(e){status.textContent=e.message;return;}"
"let button=document.querySelector('#install');installing=true;button.disabled=picker.disabled=appname.disabled=appid.disabled=true;stage('Checking available space...',0);status.textContent='';"
"try{let preflight=await fetch('/api/preflight',{method:'POST',headers:{'X-Passport-Token':token},body:body.slice(0,512)});"
"if(!preflight.ok){status.textContent=await preflight.text();stage('Installation not started. See the error below.',0);return;}"
"stage('Uploading BIN: 0%',0);await upload(body)}catch(e){status.textContent=e.message;stage('Installation interrupted. Completion was not confirmed.',0)}"
"finally{installing=false;button.disabled=picker.disabled=appname.disabled=appid.disabled=false;await list().catch(()=>{})}};"
"async function list(){let r=await fetch('/api/apps');if(!r.ok)return;let data=await r.json();apps.replaceChildren();"
"for(let a of data){let row=document.createElement('article'),name=document.createElement('b'),info=document.createElement('small');"
"const states={'ready':'Installed','settings retained':'Cleanup needed','incomplete - retry install':'Install interrupted'};"
"name.textContent=a.name;info.textContent=a.id+' | '+a.version+' | '+(states[a.state]||a.state);row.append(name,info);"
"let uninstall=document.createElement('button');uninstall.textContent='Uninstall';uninstall.className='danger';"
"uninstall.onclick=()=>{if(!installing&&confirm('Uninstall '+a.name+'? All saved data and settings will be permanently erased. This cannot be undone.'))action('/api/uninstall?slot='+a.slot,'',a.id)};"
"row.append(uninstall);if(a.id==='org.passportos.qso-vision'&&a.state==='ready'){"
"let data=document.createElement('input');data.type='file';data.accept='.qso-settings';let restore=document.createElement('button');restore.textContent='Import legacy settings';"
"restore.onclick=async()=>{if(installing)return;let f=data.files[0];if(f&&confirm('Stage legacy settings? Existing private records will be refused.')){try{await action('/api/qso-import?slot='+a.slot,f)}catch(e){status.textContent='Connection interrupted. Retry the same settings file.'}}};row.append(data,restore)}apps.append(row)}"
"let capacity=await fetch('/api/capacity');if(capacity.ok){let c=await capacity.json();document.querySelector('#capacity').textContent=(c.free_bytes/1048576).toFixed(2)+' MiB free of '+(c.total_bytes/1048576).toFixed(2)+' MiB app storage. Largest new ROM now: '+(c.largest_image_bytes/1048576).toFixed(2)+' MiB after reserving settings. Updates are checked separately.'}}list();</script>";

static const char about_html[] =
"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>About - Passport Launcher</title><style>body{font:16px system-ui;max-width:620px;margin:32px auto;padding:20px;background:#0c151d;color:#f3f7fa}"
"article{background:#192a35;padding:24px;border-radius:20px;margin:16px 0}a,.callsign{color:#c3f77a}.callsign{font-size:36px;font-weight:800;margin:8px 0}small{color:#9bb2c0}</style>"
"<a href=/>Back to ROM library</a><h1>About Passport Launcher</h1><article><small>Developed by</small><p class=callsign>9W3SPY</p>"
"<p>App version: ";
static const char about_tail[] =
"</p><p>Build platform: ESP-IDF 5.5.3<br>Hardware: FoloToy AI-Passport / ESP32-C3 / 8 MB</p><p><a href=https://github.com/lim3soft/passport_launcher target=_blank rel=noopener>Developer GitHub</a></p></article>"
"<article><h2>Open source</h2><p>Passport Launcher uses the MIT license: use, modify and redistribute it while retaining the copyright and license notices. It is provided without warranty.</p>"
"<p>FoloToy board support: MIT<br>ESP-IDF: Apache-2.0<br>LVGL: MIT</p><p>Other components retain their own licenses and copyright notices.</p>"
"<a href=/licenses.txt>Open-source licenses and full notices</a></article>"
"<article><small>Inspired by</small><h2><a href=https://github.com/bmorcelli/Launcher>M5 Launcher</a></h2><p>bmorcelli/Launcher</p></article>";

static esp_err_t about(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send_chunk(req, about_html, sizeof(about_html)-1);
    if (err == ESP_OK) err = httpd_resp_send_chunk(req, esp_app_get_description()->version, HTTPD_RESP_USE_STRLEN);
    if (err == ESP_OK) err = httpd_resp_send_chunk(req, about_tail, sizeof(about_tail)-1);
    if (err == ESP_OK) err = httpd_resp_send_chunk(req, NULL, 0);
    return err;
}

static esp_err_t licenses(httpd_req_t *req)
{
    extern const unsigned char license_start[] asm("_binary_license_bundle_txt_gz_start");
    extern const unsigned char license_end[] asm("_binary_license_bundle_txt_gz_end");
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)license_start, license_end-license_start);
}

static esp_err_t root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    httpd_resp_send_chunk(req, html, sizeof(html) - 1);
    httpd_resp_send_chunk(req, token, strlen(token));
    httpd_resp_send_chunk(req, script, sizeof(script) - 1);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t bin_import_script(httpd_req_t *req)
{
    extern const unsigned char script_start[] asm("_binary_bin_import_js_start");
    extern const unsigned char script_end[] asm("_binary_bin_import_js_end");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)script_start, script_end-script_start-1);
}

static esp_err_t list_apps(httpd_req_t *req)
{
    passport_registry_t portal_registry;
    if (!passport_snapshot(&portal_registry)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Busy");
    }
    cJSON *array = cJSON_CreateArray();
    if (!array) return ESP_ERR_NO_MEM;
    const char *states[] = {"empty", "settings retained", "incomplete - retry install", "ready"};
    for (unsigned i = 0; i < PASSPORT_SLOTS; ++i) {
        const passport_record_t *a = &portal_registry.slots[i];
        if (a->state == PASSPORT_EMPTY) continue;
        cJSON *item = cJSON_CreateObject();
        if (!item) { cJSON_Delete(array); return ESP_ERR_NO_MEM; }
        cJSON_AddNumberToObject(item, "slot", i);
        cJSON_AddStringToObject(item, "name", a->package.name);
        cJSON_AddStringToObject(item, "id", a->package.app_id);
        cJSON_AddStringToObject(item, "version", a->package.version);
        cJSON_AddStringToObject(item, "state", states[a->state]);
        bool legacy = false;
        esp_err_t mode = passport_compat_mode(&portal_registry, i, &legacy);
        cJSON_AddStringToObject(item, "mode", mode != ESP_OK ? "metadata error" : legacy ? "community bin" : "launcher SDK");
        cJSON_AddItemToArray(array, item);
    }
    char *json = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    if (!json) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return err;
}

static esp_err_t capacity(httpd_req_t *req)
{
    passport_registry_t portal_registry;
    if (!passport_snapshot(&portal_registry)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Busy");
    }
    passport_capacity_t c;passport_capacity(&portal_registry,&c);
    char json[256];
    snprintf(json,sizeof(json),"{\"total_bytes\":%u,\"free_bytes\":%u,\"largest_image_bytes\":%u,\"used_image_bytes\":%u,\"allocated_bytes\":%u}",
        (unsigned)c.total_bytes,(unsigned)c.free_bytes,(unsigned)c.largest_image_bytes,(unsigned)c.used_image_bytes,(unsigned)c.allocated_bytes);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t import_error(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > 160) return ESP_ERR_INVALID_ARG;
    char body[161]; size_t received=0;
    int64_t deadline=esp_timer_get_time()+10000000;
    while (received<req->content_len) {
        if (esp_timer_get_time()>=deadline) return ESP_FAIL;
        int n=httpd_req_recv(req,body+received,req->content_len-received);
        if (n<=0) return ESP_FAIL;
        received+=n;
    }
    body[received]=0;
    cJSON *data=cJSON_Parse(body);
    if (!data) return ESP_ERR_INVALID_ARG;
    cJSON *code=cJSON_GetObjectItemCaseSensitive(data,"code"), *bytes=cJSON_GetObjectItemCaseSensitive(data,"bytes");
    esp_err_t err=ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(code) && cJSON_IsNumber(bytes) && bytes->valuedouble>=0 && bytes->valuedouble<=0x800000) {
        passport_import_notice_t note=!strcmp(code->valuestring,"too_large")?IMPORT_TOO_LARGE:
            !strcmp(code->valuestring,"layout")?IMPORT_LAYOUT:IMPORT_INVALID;
        passport_import_note(note,(unsigned)bytes->valuedouble);err=ESP_OK;
    }
    cJSON_Delete(data);return err;
}

static esp_err_t mutation(httpd_req_t *req)
{
    char submitted[33];
    if (httpd_req_get_hdr_value_str(req, "X-Passport-Token", submitted, sizeof(submitted)) != ESP_OK ||
        strcmp(submitted, token)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid session");
    esp_err_t err;
    if (!strcmp(req->uri, "/api/install-bin")) err = passport_install_http(req);
    else if (!strcmp(req->uri, "/api/preflight")) {
        err=passport_preflight_http(req);
        if (err==ESP_ERR_INVALID_STATE) {
            httpd_resp_set_status(req,"503 Service Unavailable");
            return httpd_resp_sendstr(req,"Launcher busy or storage recovery needed. Try again after returning to the library.");
        }
        char message[96];passport_import_message(message,sizeof(message));
        if (err==ESP_OK) return httpd_resp_sendstr(req,message);
        if (err!=ESP_ERR_INVALID_VERSION) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,message);
    } else if (!strcmp(req->uri, "/api/import-error")) {
        err=import_error(req);
        if (err==ESP_OK) return httpd_resp_sendstr(req,"Validation message displayed. No app data was written.");
    }
    else {
        char query[32], value[4];
        char *end;unsigned long slot;
        bool importing = !strncmp(req->uri, "/api/qso-import?", 16);
        if ((!importing && req->content_len) || httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "slot", value, sizeof(value)) != ESP_OK ||
            !value[0] || (slot=strtoul(value,&end,10))>=PASSPORT_SLOTS || *end)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid slot");
        if (importing) {
            err = passport_qso_import_http(req, slot);
            if (err == ESP_OK) return httpd_resp_sendstr(req, "Settings staged. Launch QSO Vision to apply them. Keep your original data file until verified.");
            if (err == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                "Import needs a compatible QSO app with empty private settings. Existing settings were not replaced.");
            if (err == ESP_ERR_INVALID_VERSION) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                "Update QSO Vision to a package that supports legacy settings import first.");
            if (err == ESP_ERR_INVALID_CRC || err == ESP_ERR_INVALID_ARG) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                "Settings file is invalid or damaged. Export and verify it again.");
        } else {
            char expected[64];
            if (httpd_req_get_hdr_value_str(req, "X-Passport-App-ID", expected, sizeof(expected)) != ESP_OK)
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Refresh the app list before uninstalling.");
            err = passport_uninstall(slot, expected);
            if (err == ESP_OK) return httpd_resp_sendstr(req, "Uninstalled. The app and all saved data were erased.");
        }
    }
    if (err == ESP_OK) return httpd_resp_sendstr(req, "Done. Select an application on the device.");
    if (err == ESP_ERR_INVALID_VERSION) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
        "App SDK/identity mode mismatch. Use the original mode for updates, or delete that app and its settings before changing mode.");
    char message[96];
    snprintf(message, sizeof(message), "Operation failed: %s. No app was launched.", esp_err_to_name(err));
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, message);
}

esp_err_t passport_web_stop(void)
{
    passport_install_enable(false);
    if (server) { esp_err_t err = httpd_stop(server); if (err != ESP_OK) return err; server = NULL; }
    if (wifi_started) { esp_err_t err = esp_wifi_stop(); if (err != ESP_OK) return err; wifi_started = false; }
    if (wifi_initialized) { esp_err_t err = esp_wifi_deinit(); if (err != ESP_OK) return err; wifi_initialized = false; }
    if (ap) { esp_netif_destroy_default_wifi(ap); ap = NULL; }
    return ESP_OK;
}

esp_err_t passport_web_start(void)
{
    if (server) return ESP_OK;
    esp_err_t err = passport_prepare_web();
    if (err != ESP_OK) return err;
    err = nvs_flash_init(); /* No erasure on failure. */
    if (err != ESP_OK) return err;
    if (!network_initialized) {
        err = esp_netif_init();
        if (err != ESP_OK) return err;
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        network_initialized = true;
    }
    for (unsigned i = 0; i < sizeof(token) - 1; ++i) token[i] = "0123456789abcdef"[esp_random() & 15];
    ap = esp_netif_create_default_wifi_ap();
    if (!ap) return ESP_ERR_NO_MEM;
    err = esp_netif_dhcps_stop(ap);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) goto failed;
    esp_netif_ip_info_t ip = {0};
    IP4_ADDR(&ip.ip, 192, 168, 168, 1);
    IP4_ADDR(&ip.gw, 192, 168, 168, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    err = esp_netif_set_ip_info(ap, &ip);
    if (err != ESP_OK) goto failed;
    err = esp_netif_dhcps_start(ap);
    if (err != ESP_OK) goto failed;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) goto failed;
    wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto failed;
    wifi_config_t config = {.ap = {.ssid = "Passport Launcher", .ssid_len = 17, .channel = 1,
        .max_connection = 2, .authmode = WIFI_AUTH_WPA2_PSK}};
    memcpy(config.ap.password, password, sizeof(password));
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) goto failed;
    err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err != ESP_OK) goto failed;
    err = esp_wifi_start();
    if (err != ESP_OK) goto failed;
    wifi_started = true;
    /* RF is now active: keep the session token random; the chosen Wi-Fi password is fixed. */
    for (unsigned i = 0; i < sizeof(token) - 1; ++i) token[i] = "0123456789abcdef"[esp_random() & 15];
    memcpy(config.ap.password, password, sizeof(password));
    err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err != ESP_OK) goto failed;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = 16384;
    http.recv_wait_timeout = 8;
    http.max_open_sockets = 3;
    http.lru_purge_enable = true;
    http.max_uri_handlers = 16;
    err = httpd_start(&server, &http);
    if (err != ESP_OK) goto failed;
    passport_install_enable(true);
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=root},
        {.uri="/bin-import.js", .method=HTTP_GET, .handler=bin_import_script},
        {.uri="/api/apps", .method=HTTP_GET, .handler=list_apps},
        {.uri="/api/capacity", .method=HTTP_GET, .handler=capacity},
        {.uri="/api/install-bin", .method=HTTP_POST, .handler=mutation},
        {.uri="/api/preflight", .method=HTTP_POST, .handler=mutation},
        {.uri="/api/import-error", .method=HTTP_POST, .handler=mutation},
        {.uri="/api/uninstall", .method=HTTP_POST, .handler=mutation},
        {.uri="/api/qso-import", .method=HTTP_POST, .handler=mutation},
        {.uri="/about", .method=HTTP_GET, .handler=about},
        {.uri="/licenses.txt", .method=HTTP_GET, .handler=licenses},
    };
    for (unsigned i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i) {
        err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) goto failed;
    }
    return ESP_OK;
failed:
    passport_web_stop();
    return err;
}
