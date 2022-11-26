
window.windowShadeState.isEditMode

    genNumShadesDropdown();

    if (window.windowShadeState.isEditMode) {
        headStr += "<input onchange='windowNameChanged(this.value)' class='window-name-edit' type='text' name='windowNameVal' value='" + shadesData.name + "'></div>\r\n";
    }

// Function to generate the "number of shades" dropdown if required
function genNumShadesDropdown() {
    var shadesData = window.windowShadeState;
    var numShadesDropdown = document.getElementById("num-shades-dropdown");
    var listStr = "";
    if (window.windowShadeState.isEditMode) {
        // Settings
        listStr = "<div class='shade-config'>";
        listStr += 'Number of shades: ';
        listStr += '<select class="shade-num-edit" onchange="setNumShades(this.value)">';
        for (var i = 0; i < shadesData.maxShades; i++) {
            listStr += '<option value="' + (i + 1) + '" ' + (i + 1 == window.windowShadeState.numShades ? 'selected' : '') + '>' + (i + 1) + '</option>'
        }
        listStr += '</select>';
        listStr += "</div>";
    }
    numShadesDropdown.innerHTML = listStr;
}

var isEditMode = window.windowShadeState.isEditMode;

if (window.windowShadeState.isEditMode) {
    listStr += getShadeActionsHtml(shadeInfo, shadeIdx);
}

if (isEditMode) {
    exStr += "<input onchange='shadeNameChanged(" + shadeIdx + ", this.value)' class='shade-name-edit' type='text' name='shadeNameVal'" + shadeIdx + "' value=\"" + shadeInfo.name + "\">";
}

// Function called from dropdown action to change number of shades
function setNumShades(numShades) {
    console.log(numShades);
    window.windowShadeState.numShades = numShades;
    sendShadesInfo();
}

// Function called from exit text action to change shade name
function shadeNameChanged(shadeIdx, shadeName) {
    console.log("New name for shade " + shadeIdx + " = " + shadeName);
    window.windowShadeState.shades[shadeIdx].name = shadeName;
    sendShadesInfo();
}

// Function called from exit text action to change window name
function windowNameChanged(windowName) {
    console.log("New name for window " + windowName);
    window.windowShadeState.name = windowName;
    sendShadesInfo();
}

// Send new information to the shades controller and update display
// with new information received
function sendShadesInfo() {
    let shadeCfgStr = window.windowShadeState.name + "/" + window.windowShadeState.numShades + "/";
    for (let i = 0; i < window.windowShadeState.maxShades; i++) {
        shadeCfgStr += window.windowShadeState.shades[i].name;
        shadeCfgStr += "/";
    }
    callAjax("/shadecfg/" + shadeCfgStr, shadeInfoCallback);
}

requestShadeInfo(shadeInfoCallback);

function ajaxCallback() {
    requestShadeInfoAndDisplayUpdate();
}

// Tect mode create JSON string as though responding
function testCreateJsonStringFromShadesState() {
    jsonStr = '{';
    jsonStr += '"numShades":' + window.windowShadeState.numShades + ',';
    jsonStr += '"name":"' + window.windowShadeState.name + '",';
    jsonStr += '"shades": [';
    for (let i = 0; i < window.windowShadeState.numShades; i++) {
        if (i !== 0)
            jsonStr += ",";
        jsonStr += '{"name":"' + window.windowShadeState.shades[i].name + '",';
        jsonStr += '"busy":"' + window.windowShadeState.shades[i].busy + '"}';
    }
    jsonStr += "]}";
    return jsonStr;
}
// Test mode alternative to callAjax()
function testHandleCallAjax(url, callback) {
    let specificRespStr = "";
    console.log("testHandleCallAjax " + url);
    if (url === "/q") {
        callback(testCreateJsonStringFromShadesState());
    }
    else if (url.startsWith('/shadecfg')) {
        let params = url.split("/");
        window.windowShadeState.name = params[2];
        window.windowShadeState.numShades = parseInt(params[3]);
        for (let i = 0; i < window.windowShadeState.numShades; i++) {
            window.windowShadeState.shades[i].name = params[4 + i];
        }
        callback(testCreateJsonStringFromShadesState());
    }
}

extractShadesInfo(jsonResp);

        // Start regular updates of the blinds status
        updateShadesStatusAndRepeat(configObj);

        // Helper to request status update and display
function requestShadeInfoAndDisplayUpdate() {
    requestShadeInfo(updateShadesStatusAndRepeat);
}


// Callback to refresh display after status update, etc
function updateShadesStatusAndRepeat(jsonResp) {
    let shadeBusy = document.getElementsByClassName("shade-busy");
    for (let i = 0; i < shadeBusy.length; i++) {
        let shadeIdx = parseInt(shadeBusy.item(i).getAttribute("tag"));
        let exStr = window.windowShadeState.shades[shadeIdx].busy == "0" ? "hidden" : "visible";
        shadeBusy.item(i).style.visibility = exStr;
    }
    if (window.windowShadeState.refreshTimer)
        clearTimeout(window.windowShadeState.refreshTimer);
    window.windowShadeState.refreshTimer = setTimeout(requestShadeInfoAndDisplayUpdate, 2000);
}
