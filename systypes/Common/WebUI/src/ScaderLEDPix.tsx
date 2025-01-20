import React, { useEffect, useState } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderConfig } from './ScaderConfig';
import { HexColorPicker } from "react-colorful";

const scaderManager = ScaderManager.getInstance();
const API_BASE_URL = "/api/ledpix/0";
type ScaderLEDPixConfig = ScaderConfig['ScaderLEDPix'];

export default function ScaderLEDPix(props: ScaderScreenProps) {
  const scaderName = "ScaderLEDPix";
  const [config, setConfig] = useState(props.config[scaderName]);
  const [patterns, setPatterns] = useState([]);
  const [selectedColor, setSelectedColor] = useState("#ffffff");
  const [range, setRange] = useState([0, 1500]);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      setConfig(newConfig[scaderName]);
    });
    scaderManager.onStateChange(scaderName, (newState) => {
      console.log(`ledpix onStateChange`, newState);
    });
    fetchPatterns();
  }, []);

  const fetchPatterns = async () => {
    try {
      const response = await fetch(`${API_BASE_URL}/listpatterns`);
      if (!response.ok) {
        console.error("Failed to fetch patterns");
        return;
      }
      const data = await response.json();
      setPatterns(data?.patterns || []);
    } catch (error) {
      console.error("Error fetching patterns:", error);
    }
  };

  const startPattern = async (patternName: string) => {
    try {
      await fetch(`${API_BASE_URL}/pattern/${patternName}`, { method: 'GET' });
    } catch (error) {
      console.error(`Error starting pattern ${patternName}:`, error);
    }
  };

  const clearPattern = async () => {
    try {
      await fetch(`${API_BASE_URL}/pattern/`, { method: 'GET' });
    } catch (error) {
      console.error("Error clearing pattern:", error);
    }
  };

  const handleColorChange = (color: string) => {
    setSelectedColor(color);
    sendRangeCommand(range[0], range[1], color);
  };

  const handleRangeChange = (event: any, index: number) => {
    const newRange = [...range];
    newRange[index] = parseInt(event.target.value, 10);
    if (newRange[0] > newRange[1]) newRange[index === 0 ? 1 : 0] = newRange[index];
    setRange(newRange);
    sendRangeCommand(newRange[0], newRange[1], selectedColor);
  };

  const sendRangeCommand = async (start: number, end: number, color: string) => {
    try {
      await fetch(`${API_BASE_URL}/clear`, { method: 'GET' });
      await fetch(`${API_BASE_URL}/setall/${color.replace('#', '')}/${start}/${end}`, { method: 'GET' });
    } catch (error) {
      console.error(`Error sending range command:`, error);
    }
  };

  const editModeScreen = () => {
    return (
      <div className="ScaderElem-edit">
        <div className="ScaderElem-editmode">
          <label>
            <input
              className="ScaderElem-checkbox"
              type="checkbox"
              checked={config.enable}
              onChange={(event) => {
                const newConfig = { ...config, enable: event.target.checked };
                setConfig(newConfig);
                scaderManager.getMutableConfig()[scaderName] = newConfig;
              }}
            />
            Enable {scaderName}
          </label>
        </div>
      </div>
    );
  };

  const normalModeScreen = () => {
    return (
      config.enable && (
        <div className="ScaderElem">
          <div className="ScaderElem-section-horiz">
                      <h3>Patterns</h3>
          <div className="patterns">
            {patterns.map((pattern, index) => (
              <button className="ScaderElem-button button-small" key={index} onClick={() => startPattern(pattern)}>{pattern}</button>
            ))}
            <button className="ScaderElem-button button-small" onClick={clearPattern}>Clear</button>
          </div>
          </div>

          <div className="ScaderElem-section-horiz">
          <h3>Set LED Range</h3>
          <div className="color-picker" style={{ textAlign: 'center', marginBottom: '20px' }}>
            <HexColorPicker color={selectedColor} onChange={handleColorChange} />
          </div>
          <div className="slider-range" style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '10px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
              <label>
                Start: <input className='ScaderElem-input-inline' type="number" min="0" max="1500" value={range[0]} onChange={(e) => handleRangeChange(e, 0)} />
              </label>
              <label>
                End: <input className='ScaderElem-input-inline' type="number" min="0" max="1500" value={range[1]} onChange={(e) => handleRangeChange(e, 1)} />
              </label>
            </div>
            <div style={{ display: 'flex', gap: '20px', justifyContent: 'center' }}>
              <label>
                Start: <input type="range" min="0" max="1500" value={range[0]} onChange={(e) => handleRangeChange(e, 0)} />
              </label>
              <label>
                End: <input type="range" min="0" max="1500" value={range[1]} onChange={(e) => handleRangeChange(e, 1)} />
              </label>
            </div>
          </div>
          </div>
        </div>
      )
    );
  };

  return props.isEditingMode ? editModeScreen() : normalModeScreen();
}
