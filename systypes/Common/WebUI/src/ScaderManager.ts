import { ScaderConfig } from "./ScaderConfig";
import { ScaderStateType } from "./ScaderState";

export class ScaderManager {

    // Singleton
    private static _instance: ScaderManager;

    // Test server path
    private _testServerPath = "";

    // Server address
    private _serverAddressPrefix = "";

    // URL prefix
    private _urlPrefix: string = "/api";

    // Scader configuration
    private _scaderConfig: ScaderConfig = new ScaderConfig();

    // Modified configuration
    private _mutableConfig: ScaderConfig = new ScaderConfig();

    // System version info
    private _systemVersion: string = "";

    // Version change callbacks
    private _versionChangeCallbacks: Array<(version: string) => void> = [];

    // Config change callbacks
    private _configChangeCallbacks: Array<(config: ScaderConfig) => void> = [];

    // State change callbacks
    private _stateChangeCallbacks: { [key: string]: (state: ScaderStateType) => void } = {};

    // Last time we got a state update
    private _lastStateUpdate: number = 0;
    private MAX_TIME_BETWEEN_STATE_UPDATES_MS: number = 60000;

    // Websocket
    private _websocket: WebSocket | null = null;

    // Get instance
    public static getInstance(): ScaderManager {
        if (!ScaderManager._instance) {
            ScaderManager._instance = new ScaderManager();
        }
        return ScaderManager._instance;
    }

    public setTestServerPath(path: string): void {
        this._testServerPath = path;
    }

    // Constructor
    private constructor() {
        console.log("ScaderManager constructed");
    }

    ////////////////////////////////////////////////////////////////////////////
    // Send REST commands
    ////////////////////////////////////////////////////////////////////////////

    async sendCommand(cmd: string): Promise<Response | null> {
        try {
            const sendCommandResponse = await fetch(this._serverAddressPrefix + this._urlPrefix +
                    (cmd.startsWith("/") ? cmd : "/" + cmd));
            if (!sendCommandResponse.ok) {
                console.log(`ScaderManager sendCommand response not ok ${sendCommandResponse.status}`);
            }
            return sendCommandResponse;
        } catch (error) {
            console.log(`ScaderManager sendCommand error ${error}`);
            return null;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Init
    ////////////////////////////////////////////////////////////////////////////

    public async init(): Promise<boolean> {
        // Check if already initialized
        if (this._websocket) {
            console.log(`ScaderManager init already initialized`)
            return true;
        }
        console.log(`ScaderManager init - first time`)

        await this.getAppSettingsAndCheck();
        await this.getVersionInfo();

        const rslt = await this.connectWebSocket();

        // Start timer to check for websocket reconnection
        setInterval(async () => {
            if (!this._websocket) {
                console.log(`ScaderManager init - reconnecting websocket`);
                await this.connectWebSocket();
            }
            else if ((Date.now() - this._lastStateUpdate) > this.MAX_TIME_BETWEEN_STATE_UPDATES_MS) {
                const inactiveTimeSecs = ((Date.now() - this._lastStateUpdate) / 1000).toFixed(1);
                if (this._websocket) {
                    console.log(`ScaderManager init - closing websocket due to ${inactiveTimeSecs}s inactivity`);
                    this._websocket.close();
                    this._websocket = null;
                }
            }
            console.log(`websocket state ${this._websocket?.readyState}`);
        }, 5000);

        return rslt;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Get the configuration from the main server
    ////////////////////////////////////////////////////////////////////////////

    private async getAppSettingsAndCheck(): Promise<boolean> {

        // Get the configuration from the main server
        const appSettingsResult = await this.getAppSettings("");
        if (!appSettingsResult) {
            
            // See if test-server is available
            const appSettingAlt = await this.getAppSettings(this._testServerPath);
            if (!appSettingAlt) {
                console.log("ScaderManager init unable to get app settings");
                return false;
            }
            this._serverAddressPrefix = this._testServerPath;
        }
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Open websocket
    ////////////////////////////////////////////////////////////////////////////

    private async connectWebSocket(): Promise<boolean> {
        // Open a websocket to the server
        try {
            console.log(`ScaderManager init location.origin ${window.location.origin} ${window.location.protocol} ${window.location.host} ${window.location.hostname} ${window.location.port} ${window.location.pathname} ${window.location.search} ${window.location.hash}`)
            let webSocketURL = this._serverAddressPrefix;
            if (webSocketURL.startsWith("http")) {
                webSocketURL = webSocketURL.replace(/^http/, 'ws');
            } else {
                webSocketURL = window.location.origin.replace(/^http/, 'ws');
            }
            webSocketURL += "/scader";
            console.log(`ScaderManager init opening websocket ${webSocketURL}`);
            this._websocket = new WebSocket(webSocketURL);
            if (!this._websocket) {
                console.error("ScaderManager init unable to create websocket");
                return false;
            }
            this._websocket.binaryType = "arraybuffer";
            this._lastStateUpdate = Date.now();
            this._websocket.onopen = () => {
                // Debug
                console.log(`ScaderManager init websocket opened to ${webSocketURL}`);

                // Send subscription request messages after a short delay
                setTimeout(() => {

                    // Subscribe to scader messages
                    for (const [key] of Object.entries(this._scaderConfig)) {
                        const subscribeName = key;
                        if (subscribeName !== "ScaderCommon") {
                            console.log(`ScaderManager init subscribing to ${subscribeName}`);
                            if (this._websocket) {
                                this._websocket.send(JSON.stringify({
                                    cmdName: "subscription",
                                    action: "update",
                                    pubRecs: [
                                        {name: subscribeName, msgID: subscribeName, rateHz: 0.1},
                                    ]
                                }));
                            }
                        }
                    }
                }, 1000);
            }
            this._websocket.onmessage = (event) => {
                let data: ScaderStateType;
                try {
                    data = JSON.parse(event.data) as ScaderStateType;
                    console.log(`ScaderManager websocket message ${JSON.stringify(data)}`);
                } catch (error) {
                    console.error(`ScaderManager websocket message error ${error} msg ${event.data}`);
                    return;
                }
                // Update the last state update time
                this._lastStateUpdate = Date.now();
                // Check module in message
                if (!data.module) {
                    console.log(`ScaderManager init websocket message no type`);
                    return;
                }
                // Check module callback is registered
                if (!this._stateChangeCallbacks[data.module]) {
                    console.log(`ScaderManager init websocket message no callback for ${data.module}`);
                    return;
                }
                // Use the callback to update the state
                this._stateChangeCallbacks[data.module](data);
            }
            this._websocket.onclose = () => {
                console.log(`ScaderManager websocket closed`);
                this._websocket = null;
            }
            this._websocket.onerror = (error) => {
                console.log(`ScaderManager websocket error ${error}`);
            }
        }
        catch (error) {
            console.log(`ScaderManager websocket error ${error}`);
            return false;
        }
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Config handling
    ////////////////////////////////////////////////////////////////////////////

    // Get the config
    public getConfig(): ScaderConfig {
        return this._scaderConfig;
    }

    // Get mutable config
    public getMutableConfig(): ScaderConfig {
        return this._mutableConfig;
    }

    // Revert configuration
    public revertConfig(): void {
        this._mutableConfig = JSON.parse(JSON.stringify(this._scaderConfig));
        this._configChangeCallbacks.forEach(callback => {
            callback(this._scaderConfig);
        });
    }

    // Persist configuration
    public async persistConfig(): Promise<void> {

        // Check if ScaderCommon/hostname is changed
        if (this._mutableConfig.ScaderCommon.hostname !== this._scaderConfig.ScaderCommon.hostname) {
            // Send the new hostname to the server
            await this.setFriendlyName(this._mutableConfig.ScaderCommon.hostname);
        }
        this._scaderConfig = JSON.parse(JSON.stringify(this._mutableConfig));
        await this.postAppSettings();

        // Inform screens of config change
        this._configChangeCallbacks.forEach(callback => {
            callback(this._scaderConfig);
        });
    }

    // Check if config changed
    public isConfigChanged(): boolean {
        return JSON.stringify(this._scaderConfig) !== JSON.stringify(this._mutableConfig);
    }

    // Register a config change callback
    public onConfigChange(callback: (config:ScaderConfig) => void): void {
        // Add the callback
        this._configChangeCallbacks.push(callback);
    }

    // Register a state change callback
    public onStateChange(moduleName: string, callback: (state: ScaderStateType) => void): void {
        // Add the callback
        this._stateChangeCallbacks[moduleName] = callback;
    }
    
    ////////////////////////////////////////////////////////////////////////////
    // Get version info from the server
    ////////////////////////////////////////////////////////////////////////////

    private async getVersionInfo(): Promise<void> {
        try {
            const versionResponse = await fetch(this._serverAddressPrefix + this._urlPrefix + "/v");
            if (versionResponse && versionResponse.ok) {
                const versionData = await versionResponse.json();
                console.log(`ScaderManager getVersionInfo response: ${JSON.stringify(versionData)}`);
                // Try different response formats
                let version = "";
                if (versionData.SystemVersion) {
                    version = versionData.SystemVersion;
                } else if (versionData.v) {
                    version = versionData.v;
                } else if (typeof versionData === "string") {
                    version = versionData;
                }
                if (version) {
                    console.log(`ScaderManager setting version to: ${version}`);
                    this._systemVersion = version;
                    // Notify callbacks
                    this._versionChangeCallbacks.forEach(callback => callback(version));
                } else {
                    console.log(`ScaderManager could not extract version from response`);
                }
            } else {
                console.log(`ScaderManager getVersionInfo response not ok: ${versionResponse?.status}`);
            }
        } catch (error) {
            console.log(`ScaderManager getVersionInfo error: ${error}`);
        }
    }

    // Get system version
    public getSystemVersion(): string {
        return this._systemVersion;
    }

    // Register version change callback
    public onVersionChange(callback: (version: string) => void): void {
        this._versionChangeCallbacks.push(callback);
        // If version is already available, call immediately
        if (this._systemVersion) {
            callback(this._systemVersion);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Get the app settings from the server
    ////////////////////////////////////////////////////////////////////////////

    async getAppSettings(serverAddr:string) : Promise<boolean> {
        // Get the app settings
        console.log(`ScaderManager getting app settings`);
        let getSettingsResponse = null;
        try {
            getSettingsResponse = await fetch(serverAddr + this._urlPrefix + "/getsettings/nv");
            if (getSettingsResponse && getSettingsResponse.ok) {
                const settings = await getSettingsResponse.json();

                console.log(`ScaderManager getAppSettings ${JSON.stringify(settings)}`)

                if ("nv" in settings) {

                    // Start with a base config
                    const configBase = new ScaderConfig();

                    console.log(`ScaderManager getAppSettings empty config ${JSON.stringify(configBase)}`);

                    // Add in the non-volatile settings
                    this.addNonVolatileSettings(configBase, settings.nv);

                    console.log(`ScaderManager getAppSettings config with nv ${JSON.stringify(configBase)}`);

                    // Extract non-volatile settings
                    this._scaderConfig = configBase;
                    this._mutableConfig = JSON.parse(JSON.stringify(configBase));

                    // Inform screens of config change
                    this._configChangeCallbacks.forEach(callback => {
                        callback(this._scaderConfig);
                    });
                } else {
                    alert("ScaderManager getAppSettings settings missing nv section");
                }
                return true;
            } else {
                alert("ScaderManager getAppSettings response not ok");
            }
        } catch (error) {
            console.log(`ScaderManager getAppSettings ${error}`);
            return false;
        }
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Post applicaton settings
    ////////////////////////////////////////////////////////////////////////////

    async postAppSettings(): Promise<boolean> {
        try {
            const postSettingsURI = this._serverAddressPrefix + this._urlPrefix + "/postsettings/reboot";
            const postSettingsResponse = await fetch(postSettingsURI, 
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify(this._scaderConfig).replace("\n", "\\n")
                }
            );

            console.log(`ScaderManager postAppSettings posted ${JSON.stringify(this._scaderConfig)}`)

            if (!postSettingsResponse.ok) {
                console.error(`ScaderManager postAppSettings response not ok ${postSettingsResponse.status}`);
            }
            return postSettingsResponse.ok;
        } catch (error) {
            console.error(`ScaderManager postAppSettings error ${error}`);
            return false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Add non-volatile settings to the config
    ////////////////////////////////////////////////////////////////////////////

    private addNonVolatileSettings(config:ScaderConfig, nv:ScaderConfig) {
        // Iterate over keys in nv
        let key: keyof ScaderConfig;
        for (key in nv) {
            // Check if key exists in config
            if (!(key in config)) {
                console.log(`ScaderManager addNonVolatileSettings key ${key} not in config`);
                continue;
            }
            Object.assign(config[key], nv[key])
        }

        // if ("ScaderRelays" in nv) {
        //     nv["ScaderRelays"] = Object.assign(config.ScaderRelays, nv["ScaderRelays"]);
        // }
        // config = Object.assign(config, nv);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Set the friendly name for the device
    ////////////////////////////////////////////////////////////////////////////

    public async setFriendlyName(friendlyName:string): Promise<void> {
        try {
            await fetch(this._serverAddressPrefix + this._urlPrefix + "/friendlyname/" + friendlyName);
        } catch (error) {
            console.log(`ScaderManager setFriendlyName ${error}`);
        }
    }
}
