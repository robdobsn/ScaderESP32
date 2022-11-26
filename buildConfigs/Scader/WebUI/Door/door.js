        ////////////////////////////////////////////////////////////////////////////
        // Door (door.js)
        ////////////////////////////////////////////////////////////////////////////

        // Called on page load
        function doorBodyIsLoaded() {
            window.doorState = {
                isEnabled: false,
                maxDoors: 2,
                name: "Door",
                doors: [
                    { name: "Door1", num: 1, busy: false },
                    { name: "Door2", num: 2, busy: false },
                ],
                isUIInSetupMode: false,
                userNum: "0000",
                userPin: "0000"
            };
        }

        // Called when config information is received
        function doorConfigInfo(configInfo) {
            if ("ScaderDoor" in configInfo) {
                const configObj = configInfo["ScaderDoor"];
                console.log("Door config with", configObj);
                doorExtractInfo(configObj);
                if (window.doorState.isEnabled) {
                    doorGenHeadings();
                    doorGenGrid();
                    doorGenButtons();
                }
            }
        }

        // Extract information from the JSON provided
        function doorExtractInfo(configObj) {
            window.doorState.isEnabled = configObj.enable;
            window.doorState.name = configObj.name;
            window.doorState.doors = [];
            for (let i = 0; i < configObj.doors.length; i++) {
                window.doorState.doors.push(
                    {
                        name: configObj.doors[i].name,
                        num: i,
                    }
                );
            }
        }

        // Generate headings for the display
        function doorGenHeadings() {
            let headerDiv = document.getElementById("main-header");
            if (headerDiv.innerHTML.length > 0) {
                headerDiv.innerHTML += " / ";
            }
            headerDiv.innerHTML += window.doorState.name;
        }

        // Generate the grid containing the display info
        function doorGenGrid() {
            let doorData = window.doorState;
            let doorDiv = document.getElementById("door-grid");
            let listStr = "";
            for (let doorIdx = 0; doorIdx < doorData.doors.length; doorIdx++) {
                listStr += "<div class='items-column'>\r\n";
                let doorInfo = doorData.doors[doorIdx];
                listStr += doorGetNameHtml(doorInfo, doorIdx);
                listStr += doorGetStatusHtml(doorInfo, doorIdx);
                listStr += '</div>\r\n';
            }
            doorDiv.innerHTML = listStr;
        }

        function doorGenButtons() {
            let doorDiv = document.getElementById("door-buttons");
            doorDiv.innerHTML = doorGetButtonHtml();
        }

        // Form the html for door name
        function doorGetNameHtml(doorInfo, doorIdx) {
            let exStr = "";
            exStr += "<div class='item-name'>";
            exStr += doorInfo.name;
            exStr += "</div>\r\n";
            return exStr;
        }

        // Form the html for door status
        function doorGetStatusHtml(doorInfo, doorIdx) {
            let exStr = `<div id="doorStatusId${doorIdx}" class='item-list-line door-status-on'>\r\n`;
            exStr += `<div class='items-list-item door-lock-status'>\r\n`;
            exStr += "<p class='door-text-locked'>Locked</p>\r\n";
            exStr += "<p class='door-text-locked'>Closed</p>\r\n";
            exStr += "</div>\r\n"
            exStr += "</div>\r\n"
            return exStr;
        }

        // Form the html for shade movements
        function doorGetButtonHtml() {
            let exStr = "";
            // Unlock
            exStr += doorGetActionsButtonsHtml("Unlock All", urlAPIPrefix() + "/door/all/unlock/" + window.doorState.userNum + "/" + window.doorState.userPin);
            // let exStr = "<div class='shade-list-line'>\r\n";
            // let dirnStrs = ['up', 'stop', 'down'];
            // for (let i = 0; i < dirnStrs.length; i++) {
            //     exStr += "<div class='items-list-item'>\r\n";
            //     if (isUIInSetupMode) {
            //         exStr += "<a href='javascript:void(0)' onmousedown=\"ajaxGet('/api/shade/" + doorInfo.num + "/" + dirnStrs[i] + "/on',shadesMoveCB)\" " +
            //             "onmouseup=\"ajaxGet('/api/shade/" + doorInfo.num + "/" + dirnStrs[i] + "/off',shadesMoveCB)\">";
            //     }
            //     else {
            //         exStr += "<a href='javascript:void(0)' onclick=\"ajaxGet('/api/shade/" + doorInfo.num + "/" + dirnStrs[i] + "/pulse',shadesMoveCB)\">";
            //     }
            //     exStr += "<svg width='70' height='70'><use xlink:href = '#round-" + dirnStrs[i] + "-icon'></use></svg></a>\r\n";
            //     exStr += "</div>\r\n";
            // }
            // exStr += "</div>\r\n"
            return exStr;
        }

        // // form the html for advanced shades commands
        // function shadesGetActionsHtml(doorInfo, doorIdx) {
        //     let exStr = "";

        //     // Reset memory
        //     exStr += shadesGetActionsButtonsHtml("Clear motor memory",
        //         urlAPIPrefix() + "/shade/" + doorInfo.num + "/resetmemory");

        //     // Reverse direction
        //     exStr += shadesGetActionsButtonsHtml("Change motion direction",
        //         urlAPIPrefix() + "/shade/" + doorInfo.num + "/reversedirn");

        //     // Set up limit
        //     exStr += shadesGetActionsButtonsHtml("Set Up Limit ...",
        //         urlAPIPrefix() + "/shade/" + doorInfo.num + "/setuplimit");

        //     // Set down limit
        //     exStr += shadesGetActionsButtonsHtml("... Down Limit + Save",
        //         urlAPIPrefix() + "/shade/" + doorInfo.num + "/setdownlimit");

        //     // Set favourite position
        //     exStr += shadesGetActionsButtonsHtml("Set favourite position",
        //         urlAPIPrefix() + "/shade/" + doorInfo.num + "/setfavourite");

        //     return exStr;
        // }

        // Generate actio button HTML
        function doorGetActionsButtonsHtml(buttonText, ajaxOnClick) {
            let exStr = "";
            exStr += "<div class='items-list-line'>\r\n"
            exStr += "<div class='items-list-item'>\r\n";
            exStr += "<button onclick='ajaxGet(\"" + ajaxOnClick + "\",doorActionCB)'>" +
                buttonText + "</button>\r\n";
            exStr += "</div>\r\n";
            exStr += "</div>\r\n";
            return exStr;
        }

        // // Function called from menu button to switch into setup mode
        // function shadesToggleSetupMode() {
        //     window.doorState.isUIInSetupMode = !window.doorState.isUIInSetupMode;
        //     shadesGenGrid();
        //     shadesShowSetupModeToggle();
        // }

        // function shadesShowSetupModeToggle() {
        //     let setupStatus = window.doorState.isUIInSetupMode ? "SETUP MODE" : "";
        //     const setupModeSpace = document.getElementById("setup-mode-space");
        //     setupModeSpace.innerHTML = '<p class="shades-setup">' + setupStatus + '</p><a href="#" onclick="toggleSetupMode()"><svg width="30" height="30"><use xlink:href="#sliders-icon"></use></svg></a>';
        // }

        function doorActionCB() {
            console.log("Door action done");
        }


        // // Called when update information is received
        // function shadesUpdateInfo(updateInfo) {
        //     if ("ScaderShades" in updateInfo) {
        //         console.log("Shades update from", updateInfo);
        //     }
        //     //     let shadeBusy = document.getElementsByClassName("shade-busy");
        //     // for (let i = 0; i < shadeBusy.length; i++) {
        //     //     let doorIdx = parseInt(shadeBusy.item(i).getAttribute("tag"));
        //     //     let exStr = window.doorState.shades[doorIdx].busy == "0" ? "hidden" : "visible";
        //     //     shadeBusy.item(i).style.visibility = exStr;
        //     // }
        //     // if (window.doorState.refreshTimer)
        //     //     clearTimeout(window.doorState.refreshTimer);
        //     // window.doorState.refreshTimer = setTimeout(requestdoorInfoAndDisplayUpdate, 2000);
        // }
