# RaftWebRespFile Error Investigation
## Error: `E (72375) RaftWebRespFile: constructor connId 50 failed to start filepath /local/api/ledpix/0/listpatterns`

## Date: December 17, 2025

## Problem Analysis

### What's Happening
The web server is trying to handle the REST API request `/local/api/ledpix/0/listpatterns` as if it were a static file, creating a `RaftWebResponderFile` object instead of routing it to the REST API handler callback.

### Error Location
**File:** `RaftWebResponderFile.cpp` line 82
```cpp
#ifdef WARN_RESPONDER_FILE
    if (!_isActive)
    {
        LOG_E(MODULE_PREFIX, "constructor connId %d failed to start filepath %s",
                    _reqParams.connId, filePath.c_str());
    }
#endif
```

This error occurs when `_fileChunker.start()` returns false, meaning the file doesn't exist (because it's not a file at all - it's an API endpoint).

### Root Cause

The web server has **two handlers** that could match this request:

1. **RaftWebHandlerRestAPI** - Should handle `/api/*` endpoints via callbacks
2. **RaftWebHandlerStaticFiles** - Handles serving files from filesystem

**The Problem:** The `RaftWebHandlerStaticFiles` is incorrectly matching and attempting to serve `/local/api/ledpix/0/listpatterns` as a file.

### How the Routing Works

From `RaftWebHandlerStaticFiles.cpp`:
```cpp
// Check the URL is valid
RaftJson::NameValuePair longestMatchedPath;
for (auto& servePath : _servedPathPairs)
{
    // Check if the path matches
    if (requestHeader.URL.startsWith(servePath.name))
    {
        if (servePath.name.length() > longestMatchedPath.name.length())
            longestMatchedPath = servePath;
    }
}
```

**The static file handler uses prefix matching** - it checks if the URL **starts with** any configured serve path.

### Configuration Issue

Likely configuration in `SysTypes.json` or similar:
```json
"webServer": {
    "staticPaths": "/local:/",  // or similar
}
```

If `/local` is mapped to serve static files from root `/`, then:
- Request: `/local/api/ledpix/0/listpatterns`
- Matches: `/local` prefix
- Handler tries to open file: `/api/ledpix/0/listpatterns`
- File doesn't exist → Error

### Handler Priority Order

Web handlers are checked in the order they're added. The issue suggests:
1. Static file handler is added **before** REST API handler
2. OR: Static file handler's path prefix is too broad (`/local`)

## Why the API Handler Isn't Catching It

The REST API endpoint is registered as:
```cpp
endpointManager.addEndpoint("ledpix", RestAPIEndpoint::ENDPOINT_CALLBACK, 
                           RestAPIEndpoint::ENDPOINT_GET, ...);
```

This creates an endpoint at `/api/ledpix/*` (or similar based on prefix configuration).

**But** if the static file handler matches `/local/api/...` first, it never reaches the REST API handler.

## Solutions (In Priority Order)

### 1. Fix Static Path Configuration ⭐⭐⭐ RECOMMENDED
**Issue:** `/local` prefix is too broad and catches `/local/api/*` requests

**Fix:** Make static paths more specific to exclude API routes:
```json
"webServer": {
    "staticPaths": "/local/web:/, /local/assets:/assets"
}
```

Or configure API to use a different prefix:
```json
"restAPI": {
    "prefix": "/api"  // Ensure API uses /api not /local/api
}
```

### 2. Change Handler Registration Order ⭐⭐
**Issue:** Static file handler registered before REST API handler

**Fix:** Ensure REST API handler is registered **before** static file handler in RaftWebServer setup. This way API routes are checked first.

### 3. Add API Path Exclusion to Static Handler ⭐
**Issue:** Static file handler doesn't exclude API paths

**Fix:** Modify `RaftWebHandlerStaticFiles::getNewResponder()` to explicitly reject URLs containing `/api/`:
```cpp
// Check if this is an API request - don't handle with static files
if (requestHeader.URL.indexOf("/api/") >= 0)
    return nullptr;
```

## Investigation Steps

### Step 1: Check Web Server Configuration
Look for static file path configuration in:
- `SysTypes.json`
- `SysManager.cpp` setup
- Any `webServer` configuration

**Command:**
```bash
grep -r "staticPaths\|servePaths" systypes/ScaderLedsWaveshare/
```

### Step 2: Check REST API Prefix Configuration
Look for how the REST API base path is configured:
```bash
grep -r "restAPI\|urlPrefix\|/api/" systypes/ScaderLedsWaveshare/
```

### Step 3: Check Handler Registration Order
Look in the system manager or main setup code for the order handlers are added:
```cpp
// Want this order:
_webServer.addHandler(&_restAPIHandler);      // Register API first
_webServer.addHandler(&_staticFilesHandler);  // Register static files second
```

### Step 4: Verify Expected Behavior

The request `/local/api/ledpix/0/listpatterns` should either be:

**Option A:** Pure API (no `/local` prefix)
- Request: `/api/ledpix/0/listpatterns`
- Handler: REST API
- Static paths: `/local/web:/`, `/local/assets:/assets`

**Option B:** API with `/local` prefix but static paths more specific
- Request: `/local/api/ledpix/0/listpatterns`
- Handler: REST API (prefix configured as `/local/api`)
- Static paths: `/local/web:/`, `/local/assets:/assets` (more specific)

## Current Endpoint Registration

From `ScaderLEDPixels.cpp`:
```cpp
endpointManager.addEndpoint("ledpix", RestAPIEndpoint::ENDPOINT_CALLBACK, 
                           RestAPIEndpoint::ENDPOINT_GET,
                           std::bind(&ScaderLEDPixels::apiControl, ...),
                           "control LED pixels, ...");
```

This creates endpoint: `<prefix>/ledpix/<segment>/<command>`

If prefix is `/local/api`, full path becomes: `/local/api/ledpix/0/listpatterns`

## Expected vs Actual

**Expected:**
1. Request arrives: `/local/api/ledpix/0/listpatterns`
2. REST API handler matches: `/local/api/*`
3. Endpoint callback called: `ScaderLEDPixels::apiControl()`
4. Response: `{"rslt":"ok","patterns":["RainbowSnake","autoid"]}`

**Actual:**
1. Request arrives: `/local/api/ledpix/0/listpatterns`
2. Static file handler matches: `/local/*`
3. Tries to open file: `/api/ledpix/0/listpatterns`
4. File not found → Error logged
5. Returns 404 (presumably)

## Immediate Action

**Most likely fix:** The static path configuration is too broad.

**Check this file first:**
```
systypes/ScaderLedsWaveshare/SysTypes.json
```

Look for `staticPaths` or similar configuration and ensure it doesn't catch `/local/api/*` patterns.

**Expected correct configuration:**
```json
"webServer": {
    "staticPaths": "/local:/localfs, /assets:/assets"
}
```

Or ensure REST API explicitly claims `/local/api`:
```json
"restAPI": {
    "urlPrefix": "/local/api"
}
```

## Non-Breaking Workaround

If you can't change the configuration immediately, you could modify the web UI to call the API without the `/local` prefix:

**In `ScaderLEDPix.tsx` line 55:**
```typescript
// Change from:
const response = await fetch(`${API_BASE_URL}/listpatterns`);

// To (if API_BASE_URL includes /local):
const response = await fetch(`/api/ledpix/0/listpatterns`);
```

But the **proper fix is in the server configuration**, not the client.

## Summary

**Issue:** Static file handler with `/local` prefix catches API requests before REST API handler
**Cause:** Handler priority or overly broad static path configuration
**Fix:** Configure static paths more specifically to exclude `/api/` routes
**Location:** Likely in `systypes/ScaderLedsWaveshare/SysTypes.json` or equivalent configuration file

The error is harmless (just logs an error and returns 404) but indicates a routing misconfiguration that should be fixed for clarity and proper operation.
