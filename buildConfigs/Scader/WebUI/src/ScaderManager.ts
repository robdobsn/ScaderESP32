import { ScaderConfig } from "./ScaderConfig";
import { ScaderRelayStates, ScaderShadeState, ScaderShadeStates, ScaderState } from "./ScaderState";

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

    // Current state
    private _currentState: ScaderState = new ScaderState();

    // Config change callbacks
    private _configChangeCallbacks: Array<(config: ScaderConfig) => void> = [];

    // State change callbacks
    private _stateChangeCallbacks: Array<(config: ScaderRelayStates | ScaderShadeStates) => void> = [];

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

    ////////////////////////////////////////////////////////////////////////////
    // Send REST commands
    ////////////////////////////////////////////////////////////////////////////

    async sendCommand(cmd: string): Promise<boolean> {
        try {
            const sendCommandResponse = await fetch(this._serverAddressPrefix + this._urlPrefix + cmd);
            if (!sendCommandResponse.ok) {
                console.log(`sendCommand response not ok ${sendCommandResponse.status}`);
            }
            return sendCommandResponse.ok;
        } catch (error) {
            console.log(`sendCommand error ${error}`);
            return false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Init
    ////////////////////////////////////////////////////////////////////////////

    public async init(): Promise<boolean> {
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
        // Open a websocket to the server
        let ws:WebSocket | null = null;
        try {
            ws = new WebSocket(this._serverAddressPrefix.replace("http", "ws") + "/scader");
            if (!ws) {
                console.error("ScaderManager init unable to create websocket");
                return false;
            }
            ws.binaryType = "arraybuffer";

            ws.onopen = () => {
                console.log(`ScaderManager init websocket opened to ${this._serverAddressPrefix}`);
                // Subscribe to scader messages
                for (const [key, elem] of Object.entries(this._scaderConfig)) {
                    const subscribeName = key;
                    if (subscribeName !== "ScaderCommon") {
                        console.log(`ScaderManager init subscribing to ${subscribeName}`);
                        if (ws) {
                            ws.send(JSON.stringify({
                                cmdName: "subscription",
                                action: "update",
                                pubRecs: [
                                    {name: subscribeName, msgID: subscribeName, rateHz: 0.1},
                                ]
                            }));
                        }
                    }
                }            
            }
            ws.onmessage = (event) => {
                console.log(`ScaderManager init websocket message`);
                const data = JSON.parse(event.data);
                // Use the state callbacks to update the state
                this._stateChangeCallbacks.forEach(callback => {
                    callback(data);
                });
                console.log(`ScaderManager init websocket message ${JSON.stringify(data)}`);
            }
            ws.onclose = () => {
                console.log(`ScaderManager init websocket closed`);
            }
            ws.onerror = (error) => {
                console.log(`ScaderManager init websocket error ${error}`);
            }
        }
        catch (error) {
            console.log(`ScaderManager init websocket error ${error}`);
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
    public persistConfig(): void {
        this._scaderConfig = JSON.parse(JSON.stringify(this._mutableConfig));
        this.postAppSettings();
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
    public onStateChange(callback: (state:ScaderRelayStates | ScaderShadeStates) => void): void {
        // Add the callback
        this._stateChangeCallbacks.push(callback);
    }
    
    ////////////////////////////////////////////////////////////////////////////
    // Get and post the app settings from the server
    ////////////////////////////////////////////////////////////////////////////

    async getAppSettings(serverAddr:string) : Promise<boolean> {
        // Get the app settings
        console.log(`ScaderManager getting app settings`);
        let getSettingsResponse = null;
        try {
            getSettingsResponse = await fetch(serverAddr + this._urlPrefix + "/getsettings/nv");
            if (getSettingsResponse && getSettingsResponse.ok) {
                const settings = await getSettingsResponse.json();
                if ("nv" in settings) {

                    // Start with a base config
                    const configBase = new ScaderConfig();
                    Object.assign(configBase, settings.nv);

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

    // Post applicaton settings
    async postAppSettings(): Promise<boolean> {
        try {
            const postSettingsURI = this._serverAddressPrefix + this._urlPrefix + "/postsettings";
            const postSettingsResponse = await fetch(postSettingsURI, 
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify(this._scaderConfig).replace("\n", "\\n")
                }
            );
            if (!postSettingsResponse.ok) {
                console.log(`postAppSettings response not ok ${postSettingsResponse.status}`);
            }
            return postSettingsResponse.ok;
        } catch (error) {
            console.log(`postAppSettings error ${error}`);
            return false;
        }
    }
}
