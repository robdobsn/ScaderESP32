        ////////////////////////////////////////////////////////////////////////////
        // Shades (shades.js)
        ////////////////////////////////////////////////////////////////////////////

        // // Called on page load
        // function shadesBodyIsLoaded() {
        //     window.windowShadeState = {
        //         isEnabled: false,
        //         maxShades: 5,
        //         name: "Window Shades",
        //         shades: [
        //             { name: "Shade1", num: 1, busy: false },
        //             { name: "Shade2", num: 2, busy: false },
        //             { name: "Shade3", num: 3, busy: false },
        //             { name: "Shade4", num: 4, busy: false },
        //             { name: "Shade5", num: 5, busy: false }
        //         ],
        //         isUIInSetupMode: false
        //     };
        //     shadesUIUpdate(window.appState.isUIInSetupMode);
        // }

        // Update shades UI
        function shadesUIUpdate() {
            // Check shades data
            if (!("ScaderShades" in window.appState.configObj)) {
                if (!window.appState.uiInSetupMode)
                    return;
                window.appState.configObj["ScaderShades"] = shadesGetSetupConfig();
            }
            // Update UI
            shadesGenHeadings();
            shadesGenEnables();
            shadesGenGrid();
        }

        // // Called when config information is received
        // function shadesConfigInfo(configInfo) {
        //     if ("ScaderShades" in configInfo) {
        //         const configObj = configInfo["ScaderShades"];
        //         console.log("Shades config with", configObj);
        //         shadesExtractInfo(configObj);
        //         if (window.windowShadeState.isEnabled) {
        //             shadesGenHeadings();
        //             shadesGenGrid();
        //         }
        //     }
        // }

        // // Extract information from the JSON provided (which came from shades controller)
        // function shadesExtractInfo(configObj) {
        //     window.windowShadeState.isUIInSetupMode = false;
        //     window.windowShadeState.isEnabled = configObj.enable;
        //     window.windowShadeState.name = configObj.name;
        //     window.windowShadeState.shades = [];
        //     for (let i = 0; i < configObj.shades.length; i++) {
        //         window.windowShadeState.shades.push(
        //             {
        //                 name: configObj.shades[i].name,
        //                 num: i,
        //                 busy: configObj.shades[i].busy,
        //             }
        //         );
        //     }
        // }

        // Generate headings for the window shades display
        function shadesGenHeadings() {
            // let headerDiv = document.getElementById("main-header");
            // if (!window.appState.configObj.ScaderShades.enable) {
            //     return;
            // }
            // if (headerDiv.innerHTML.length > 0) {
            //     headerDiv.innerHTML += " / ";
            // }
            // headerDiv.innerHTML += window.appState.configObj.ScaderShades.name;
        }

        // Function to generate enable and "number of shades" dropdown if required
        function shadesGenEnables() {
            const numShadesDropdown = document.getElementById("shades-enables");
            let listStr = "";
            if (!window.appState.uiInSetupMode) {
                numShadesDropdown.innerHTML = "";
                return;
            } else {
                // Settings
                listStr = "<div class='common-config'>";
                listStr += '<input type="checkbox" class="common-checkbox" id="shades-enable-checkbox" name="shades-enable-checkbox" onclick="shadesEnableChange(this);" ' +
                    (window.appState.configObj.ScaderShades.enable ? "checked" : "") + '>';
                listStr += '<label for="shades-enable-checkbox">Enable shades</label>';
                if (window.appState.configObj.ScaderShades.enable) {
                    listStr += '<span class="common-dropdown-label">Number of shades:<span>';
                    listStr += '<select class="common-num-edit" onchange="shadesSetNum(this.value)">';
                    for (let i = 0; i < window.appState.configObj.ScaderShades.maxShades; i++) {
                        listStr += '<option value="' + (i + 1) + '" ' + (i + 1 == window.appState.configObj.ScaderShades.numShades ? 'selected' : '') + '>' + (i + 1) + '</option>'
                    }
                    listStr += '</select>';
                }
                listStr += "</div>";
            }
            numShadesDropdown.innerHTML = listStr;
        }

        // Generate the grid containing the window shades display info
        function shadesGenGrid() {
            let shadesDiv = document.getElementById("shades-grid");
            if (!window.appState.configObj.ScaderShades.enable) {
                shadesDiv.innerHTML = "";
                return;
            }
            let configObj = window.appState.configObj.ScaderShades;
            let listStr = "";
            for (let shadeIdx = 0; shadeIdx < configObj.numShades; shadeIdx++) {
                listStr += "<div class='items-column'>\r\n";
                let shadeInfo = configObj.shades[shadeIdx];
                listStr += shadesGetNameHtml(shadeInfo, shadeIdx);
                listStr += shadesGetMoveHtml(shadeInfo, shadeIdx);
                listStr += shadesGetBusyHtml(shadeInfo, shadeIdx);
                if (window.appState.uiInSetupMode) {
                    listStr += shadesGetActionsHtml(shadeInfo, shadeIdx);
                }
                listStr += '</div>\r\n';
            }
            shadesDiv.innerHTML = listStr;
        }

        // Form the html for shade name
        function shadesGetNameHtml(shadeInfo, shadeIdx) {
            let exStr = "";
            exStr += "<div class='item-name'>";
            if (!window.appState.uiInSetupMode) {
                exStr += shadeInfo.name;
            } else {
                exStr += "<input onchange='shadeNameChanged(" + shadeIdx + ", this.value)' class='common-name-edit' type='text' name='shadeNameVal'" + shadeIdx + "' value=\"" + shadeInfo.name + "\">";
            }
            exStr += "</div>\r\n";
            return exStr;
        }

        // Form the html for shade busy
        function shadesGetBusyHtml(shadeInfo, shadeIdx) {
            let exStr = "<div class='shade-list-line'>\r\n";
            exStr += "<div class='items-list-item'>\r\n";
            exStr += "<p tag=" + shadeIdx + " class='shade-busy'>BUSY</p>\r\n";
            exStr += "</div>\r\n"
            exStr += "</div>\r\n"
            return exStr;
        }

        // Form the html for shade movements
        function shadesGetMoveHtml(shadeInfo, shadeIdx) {
            let exStr = "<div class='shade-list-line'>\r\n";
            let dirnStrs = ['up', 'stop', 'down'];
            for (let i = 0; i < dirnStrs.length; i++) {
                exStr += "<div class='items-list-item'>\r\n";
                if (window.appState.uiInSetupMode) {
                    exStr += "<a href='javascript:void(0)' onmousedown=\"ajaxGet('/api/shade/" + (shadeIdx + 1) + "/" + dirnStrs[i] + "/on',shadesMoveCB)\" " +
                        "onmouseup=\"ajaxGet('/api/shade/" + (shadeIdx + 1) + "/" + dirnStrs[i] + "/off',shadesMoveCB)\">";
                } else {
                    exStr += "<a href='javascript:void(0)' onclick=\"ajaxGet('/api/shade/" + (shadeIdx + 1) + "/" + dirnStrs[i] + "/pulse',shadesMoveCB)\">";
                }
                exStr += "<svg width='70' height='70'><use xlink:href = '#round-" + dirnStrs[i] + "-icon'></use></svg></a>\r\n";
                exStr += "</div>\r\n";
            }
            exStr += "</div>\r\n"
            return exStr;
        }

        // form the html for advanced shades commands
        function shadesGetActionsHtml(shadeInfo, shadeIdx) {
            let exStr = "";

            // Reset memory
            exStr += shadesGetActionsButtonsHtml("Clear motor memory",
                urlAPIPrefix() + "/shade/" + (shadeIdx + 1) + "/resetmemory");

            // Reverse direction
            exStr += shadesGetActionsButtonsHtml("Change motion direction",
                urlAPIPrefix() + "/shade/" + (shadeIdx + 1) + "/reversedirn");

            // Set up limit
            exStr += shadesGetActionsButtonsHtml("Set Up Limit ...",
                urlAPIPrefix() + "/shade/" + (shadeIdx + 1) + "/setuplimit");

            // Set down limit
            exStr += shadesGetActionsButtonsHtml("... Down Limit + Save",
                urlAPIPrefix() + "/shade/" + (shadeIdx + 1) + "/setdownlimit");

            // Set favourite position
            exStr += shadesGetActionsButtonsHtml("Set favourite position",
                urlAPIPrefix() + "/shade/" + (shadeIdx + 1) + "/setfavourite");

            return exStr;
        }

        // Generate actio button HTML
        function shadesGetActionsButtonsHtml(buttonText, ajaxOnClick) {
            let exStr = "";
            exStr += "<div class='shade-list-line'>\r\n"
            exStr += "<div class='items-list-item'>\r\n";
            exStr += "<button onclick='ajaxGet(\"" + ajaxOnClick + "\",shadesMoveCB)'>" +
                buttonText + "</button>\r\n";
            exStr += "</div>\r\n";
            exStr += "</div>\r\n";
            return exStr;
        }

        function shadesMoveCB() {
            console.log("Shades move done");
        }

        // Called when update information is received
        function shadesUpdateInfo(updateInfo) {
            if ("ScaderShades" in updateInfo) {
                console.log("Shades update from", updateInfo);
            }
            //     let shadeBusy = document.getElementsByClassName("shade-busy");
            // for (let i = 0; i < shadeBusy.length; i++) {
            //     let shadeIdx = parseInt(shadeBusy.item(i).getAttribute("tag"));
            //     let exStr = window.windowShadeState.shades[shadeIdx].busy == "0" ? "hidden" : "visible";
            //     shadeBusy.item(i).style.visibility = exStr;
            // }
            // if (window.windowShadeState.refreshTimer)
            //     clearTimeout(window.windowShadeState.refreshTimer);
            // window.windowShadeState.refreshTimer = setTimeout(requestShadeInfoAndDisplayUpdate, 2000);
        }

        function shadesGetSetupConfig() {
            return {
                enable: false,
                maxShades: 5,
                numShades: 5,
                name: "Window Shades",
                shades: [
                    { name: "Shade1", busy: false },
                    { name: "Shade2", busy: false },
                    { name: "Shade3", busy: false },
                    { name: "Shade4", busy: false },
                    { name: "Shade5", busy: false }
                ]
            }
        }
        //     window.windowShadeState = {
        //         isEnabled: false,
        //         maxShades: 5,
        //         name: "Window Shades",
        //         shades: [
        //             { name: "Shade1", num: 1, busy: false },
        //             { name: "Shade2", num: 2, busy: false },
        //             { name: "Shade3", num: 3, busy: false },
        //             { name: "Shade4", num: 4, busy: false },
        //             { name: "Shade5", num: 5, busy: false }
        //         ],
        //         isUIInSetupMode: false
        //     };

        function shadesSetNum(numShades) {
            window.appState.configObj.ScaderShades.numShades = numShades;
            shadesUIUpdate();
        }

        // Function called from edit text action to change shade name
        function shadeNameChanged(shadeIdx, shadeName) {
            console.log("New name for shade " + shadeIdx + " = " + shadeName);
            window.appState.configObj.ScaderShades.shades[shadeIdx].name = shadeName;
        }

        function shadesEnableChange(elem) {
            window.appState.configObj.ScaderShades.enable = elem.checked;
            shadesUIUpdate();
        }