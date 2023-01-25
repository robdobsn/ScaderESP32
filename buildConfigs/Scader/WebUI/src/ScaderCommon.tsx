
// Header bar

import React from "react";
import { ScaderConfig } from "./ScaderConfig";
import { MenuIcon } from "./ScaderIcons";
import { ScaderManager } from "./ScaderManager";

const scaderManager = ScaderManager.getInstance();
export class ScaderScreenProps {
    constructor(
        public isEditingMode: boolean,
        public config: ScaderConfig
    ) {}
}

interface ScaderHeaderProps {
    config: ScaderConfig;
    isEditMode: boolean;
    onClickHamburger: React.MouseEventHandler<HTMLButtonElement>;
    onClickSave: React.MouseEventHandler<HTMLButtonElement>;
    onClickCancel: React.MouseEventHandler<HTMLButtonElement>;
}

export const ScaderCommon = (props:ScaderHeaderProps) => {

    const [config, setConfig] = React.useState(props.config);

    scaderManager.onConfigChange((newConfig) => {
        console.log("ScaderCommon.onConfigChange");
        // Update config
        setConfig(newConfig);
      });  

    const onTitleChange = (event: React.ChangeEvent<HTMLInputElement>) => {
        console.log(`ScaderCommon.onTitleChange ${event.target.value}`);
        // Update config
        setConfig({...config, ScaderCommon: {...config.ScaderCommon, name: event.target.value}});
        ScaderManager.getInstance().getMutableConfig().ScaderCommon = config.ScaderCommon;
    };

    const onHostnameChange = (event: React.ChangeEvent<HTMLInputElement>) => {
        console.log(`ScaderCommon.onHostnameChange ${event.target.value}`);
        setConfig({...config, ScaderCommon: {...config.ScaderCommon, hostname: event.target.value}});
        ScaderManager.getInstance().getMutableConfig().ScaderCommon = config.ScaderCommon;
    };

    const handleEthEnabledChange = (event: React.ChangeEvent<HTMLInputElement>) => {
        console.log(`ScaderCommon.handleEthEnabledChange ${event.target.checked}`);
        const netManConfig = {...config.NetMan, EthEnabled: event.target.checked ? 1 : 0};
        console.log(`ScaderCommon.handleEthEnabledChange netManConfig = ${JSON.stringify(netManConfig)}`);
        const newConfig = {...config, NetMan: netManConfig};
        console.log(`ScaderCommon.handleEthEnabledChange config = ${JSON.stringify(newConfig)}`);
        const mutableConfig = ScaderManager.getInstance().getMutableConfig();
        console.log(`ScaderCommon.handleEthEnabledChange mutableConfig = ${JSON.stringify(mutableConfig)}`);
        mutableConfig.NetMan = newConfig.NetMan;
        console.log(`ScaderCommon.handleEthEnabledChange updated mutableConfig = ${JSON.stringify(mutableConfig)}`);
        setConfig(newConfig);
    };

    const onClickSave = (event: React.MouseEvent<HTMLButtonElement>) => {
        props.onClickSave(event);
    }

    console.log(`ScaderCommon.render ${props.config.ScaderCommon && props.config.ScaderCommon.name ? props.config.ScaderCommon.name : "no name"}`);

    const headerClassName = props.isEditMode ? "ScaderHeader-bareditmode" : "ScaderHeader-bar";
    return (
        <div className="ScaderHeader">
        <div className={headerClassName}>
            {props.isEditMode ? 
                <div className="ScaderHeader-title">Edit Mode</div> :
                <div className="ScaderHeader-title">{config.ScaderCommon.name ? config.ScaderCommon.name : "Scader"}</div>
            }
            {props.isEditMode ?
                <div className="ScaderHeader-button-bar">
                    <button className="ScaderHeader-button" 
                            onClick={onClickSave}
                        >Save</button>
                    <button className="ScaderHeader-button" 
                            onClick={props.onClickCancel}
                        >Cancel</button>
                </div> :
                null
            }
            <button className="ScaderHeader-hamburger" onClick={props.onClickHamburger}>
                <div className="ScaderHeader-hamburger-icon">
                    <MenuIcon fill="#ffffff"/>
                </div>
            </button>
        </div>
        {props.isEditMode ?
            <div className="ScaderElem">
                <div className="ScaderElem-header">
                    <p>
                        <label htmlFor="scader-title">Title:</label>
                        <input className="ScaderElem-input" type="text" name="scader-title" value={config.ScaderCommon.name} onChange={onTitleChange}></input>
                    </p>
                    <p>
                        {/* Input for hostname with label */}
                        <label htmlFor="scader-hostname">Hostname:</label>
                        <input className="ScaderElem-input" type="text" name="scader-hostname" value={config.ScaderCommon.hostname} onChange={onHostnameChange}></input>
                    </p>
          <label>
            <input className="ScaderElem-checkbox" type="checkbox" 
                  checked={config.NetMan.EthEnabled === 1}
                  onChange={handleEthEnabledChange} />
            Enable Wired Ethernet
          </label>                    
                </div>
            </div>
            : null}
        </div>
  );
};
