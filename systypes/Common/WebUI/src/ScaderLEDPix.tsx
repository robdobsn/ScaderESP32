import React, { useEffect, useState } from 'react';
import { ScaderScreenProps } from './ScaderCommon';
import { ScaderManager } from './ScaderManager';
import { ScaderConfig } from './ScaderConfig';
import { HexColorPicker } from "react-colorful";

const scaderManager = ScaderManager.getInstance();
const API_BASE_URL = "/api/ledpix/0";
type ScaderLEDPixConfig = ScaderConfig['ScaderLEDPix'];

const maxLedCount = 2000;

export default function ScaderLEDPix(props: ScaderScreenProps) {
  const scaderName = "ScaderLEDPix";
  const [config, setConfig] = useState(props.config[scaderName]);
  const [patterns, setPatterns] = useState([]);
  const [selectedColor, setSelectedColor] = useState("#ffffff");
  const [range, setRange] = useState([0, maxLedCount]);
  const [immediateMode, setImmediateMode] = useState(true);
  const [pendingChanges, setPendingChanges] = useState({ color: "#ffffff", range: [0, maxLedCount] });
  const [isRequestPending, setIsRequestPending] = useState(false);
  const debounceTimerRef = React.useRef<number | null>(null);
  const latestRequestRef = React.useRef<{ start: number; end: number; color: string } | null>(null);

  useEffect(() => {
    scaderManager.onConfigChange((newConfig) => {
      setConfig(newConfig[scaderName]);
    });
    scaderManager.onStateChange(scaderName, (newState) => {
      console.log(`ledpix onStateChange`, newState);
    });
    fetchPatterns();
    
    // Cleanup debounce timer on unmount
    return () => {
      if (debounceTimerRef.current !== null) {
        window.clearTimeout(debounceTimerRef.current);
      }
    };
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
    if (immediateMode) {
      debouncedSendRangeCommand(range[0], range[1], color);
    } else {
      setPendingChanges((prev) => ({ ...prev, color }));
    }
  };

  const handleRangeChange = (event: any, index: number) => {
    const newRange = [...range];
    newRange[index] = parseInt(event.target.value, 10);
    if (newRange[0] > newRange[1]) newRange[index === 0 ? 1 : 0] = newRange[index];
    setRange(newRange);

    if (immediateMode) {
      debouncedSendRangeCommand(newRange[0], newRange[1], selectedColor);
    } else {
      setPendingChanges((prev) => ({ ...prev, range: newRange }));
    }
  };

  const applyChanges = () => {
    sendRangeCommand(pendingChanges.range[0], pendingChanges.range[1], pendingChanges.color);
  };

  const debouncedSendRangeCommand = (start: number, end: number, color: string) => {
    // Store the latest request parameters
    latestRequestRef.current = { start, end, color };

    // Clear any existing debounce timer
    if (debounceTimerRef.current !== null) {
      window.clearTimeout(debounceTimerRef.current);
    }

    // Set a new debounce timer (100ms delay)
    debounceTimerRef.current = window.setTimeout(() => {
      debounceTimerRef.current = null;
      
      // If a request is already pending, it will be picked up after completion
      if (!isRequestPending && latestRequestRef.current) {
        executeLatestRequest();
      }
    }, 100);
  };

  const executeLatestRequest = async () => {
    if (!latestRequestRef.current || isRequestPending) return;

    const { start, end, color } = latestRequestRef.current;
    latestRequestRef.current = null; // Clear the request
    
    setIsRequestPending(true);
    try {
      await sendRangeCommand(start, end, color);
    } finally {
      setIsRequestPending(false);
      
      // If another request came in while we were processing, execute it now
      if (latestRequestRef.current) {
        executeLatestRequest();
      }
    }
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
                <button className="ScaderElem-button ScaderElem-button-small" key={index} onClick={() => startPattern(pattern)}>{pattern}</button>
              ))}
              <button className="ScaderElem-button ScaderElem-button-small" onClick={clearPattern}>Clear</button>
            </div>
          </div>

          <div className="ScaderElem-section-horiz">
            <h3>Set LED Range</h3>
            <label>
              <input
                type="checkbox"
                checked={immediateMode}
                onChange={() => setImmediateMode(!immediateMode)}
                style={{ width: '20px', height: '20px' }}
              /> Immediate Mode
            </label>
            <div className="color-picker" style={{ textAlign: 'center', marginTop: '20px', marginBottom: '20px' }}>
              <HexColorPicker color={selectedColor} onChange={handleColorChange} style={{ width: '400px', height: '400px' }} />
            </div>
            <div className="slider-range" style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '10px', width: "400px" }}>

              {/* Start */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', fontSize: "2rem" }}>
                <label>
                  Start: <input className='ScaderElem-input-inline' style={{fontSize: "2rem", minWidth: "6rem"}} type="number" min="0" max={maxLedCount} value={range[0]} onChange={(e) => handleRangeChange(e, 0)} />
                </label>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', width: '100%' }}>
                  <input type="range" min="0" max={maxLedCount} value={range[0]} style={{ width: '100%' }} onChange={(e) => handleRangeChange(e, 0)} />
              </div>

              {/* End */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', fontSize: "2rem" }}>
                <label>
                  End: <input className='ScaderElem-input-inline' style={{fontSize: "2rem", minWidth: "6rem"}} type="number" min="0" max={maxLedCount} value={range[1]} onChange={(e) => handleRangeChange(e, 1)} />
                </label>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', width: '100%' }}>
                <input type="range" min="0" max={maxLedCount} value={range[1]} style={{ width: '100%' }} onChange={(e) => handleRangeChange(e, 1)} />
              </div>
            </div>
            {!immediateMode && (
              <button className="ScaderElem-button ScaderElem-button-small" style={{ marginTop: '20px' }} onClick={applyChanges}>Send</button>
            )}
          </div>
        </div>
      )
    );
  };

  return props.isEditingMode ? editModeScreen() : normalModeScreen();
}
