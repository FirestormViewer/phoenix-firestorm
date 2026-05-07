# Viewer to External Editor JSON-RPC<br>Message Interfaces Documentation

This document describes all the message interfaces defined for WebSocket communication between the Second Life viewer and an external editor such as a VSCode extension.

## Table of Contents

- [Usage Flow](#usage-flow)
- [JSON-RPC Method Summary](#json-rpc-method-summary)
- [Session Management Interfaces](#session-management-interfaces)
  - [SessionHandshake](#sessionhandshake)
  - [SessionHandshakeResponse](#sessionhandshakeresponse)
  - [Session OK](#session-ok)
  - [SessionDisconnect](#sessiondisconnect)
- [Language and Syntax Interfaces](#language-and-syntax-interfaces)
  - [SyntaxChange](#syntaxchange)
  - [Language Syntax ID Request](#language-syntax-id-request)
  - [Language Syntax Request](#language-syntax-request)
  - [Language Syntax Cache List](#language-syntax-cache-list)
  - [Language Syntax Cache Get](#language-syntax-cache-get)
- [Script Subscription Interfaces](#script-subscription-interfaces)
  - [ScriptSubscribe](#scriptsubscribe)
  - [ScriptSubscribeResponse](#scriptsubscriberesponse)
  - [ScriptUnsubscribe](#scriptunsubscribe)
  - [ScriptList](#scriptlist)
- [Compilation Interfaces](#compilation-interfaces)
  - [CompilationError](#compilationerror)
  - [CompilationResult](#compilationresult)
- [Runtime Event Interfaces](#runtime-event-interfaces)
  - [RuntimeDebug](#runtimedebug)
  - [RuntimeError](#runtimeerror)
- [Handler and Configuration Interfaces](#handler-and-configuration-interfaces)
  - [WebSocketHandlers](#websockethandlers)
  - [ClientInfo](#clientinfo)

## Usage Flow

1. **Connection Establishment:**

   - Viewer sends `session.handshake` call with `SessionHandshake` data
   - Extension responds with `SessionHandshakeResponse`
   - Viewer confirms with `session.ok` notification

2. **Language Information Exchange:**

   - Extension makes `language.syntax.id` call to get current syntax version
   - Extension makes `language.syntax` calls with different `kind` parameters to get specific language data
   - Viewer responds with a `LanguageInfo` object containing the requested definitions

3. **Script Subscription Management:**

   - Extension makes `script.subscribe` call with `ScriptSubscribe` data to request live synchronization for a script
   - Viewer responds with `ScriptSubscribeResponse` indicating success or failure
   - When subscription needs to be terminated, viewer sends `script.unsubscribe` notification with `ScriptUnsubscribe` data
   - Extension handles unsubscription by cleaning up local script tracking

4. **Runtime Events:**

   - Viewer sends `language.syntax.change` notification with `SyntaxChange` when language changes
   - Viewer sends `script.compiled` notification with `CompilationResult` after script compilation
   - Viewer sends `runtime.debug` notification with `RuntimeDebug` for debug messages during script execution
   - Viewer sends `runtime.error` notification with `RuntimeError` when runtime errors occur

5. **Connection Termination:**
   - Either side can send `session.disconnect` notification with `SessionDisconnect` data
   - Connection is closed gracefully

## JSON-RPC Method Summary

| Method                          | Direction          | Type         | Interface/Parameters       |
| ------------------------------- | ------------------ | ------------ | -------------------------- |
| `session.handshake`             | Viewer → Extension | Call         | `SessionHandshake`         |
| `session.handshake` (response)  | Extension → Viewer | Response     | `SessionHandshakeResponse` |
| `session.ok`                    | Viewer → Extension | Notification | _(no interface)_           |
| `session.disconnect`            | Bidirectional      | Notification | `SessionDisconnect`        |
| `script.subscribe`              | Extension → Viewer | Call         | `ScriptSubscribe`          |
| `script.subscribe` (response)   | Viewer → Extension | Response     | `ScriptSubscribeResponse`  |
| `script.unsubscribe`            | Viewer → Extension | Notification | `ScriptUnsubscribe`        |
| `script.list`                   | Extension → Viewer | Call         | _(no parameters)_          |
| `script.list` (response)        | Viewer → Extension | Response     | `ScriptList`               |
| `language.syntax.id`            | Extension → Viewer | Call         | _(no parameters)_          |
| `language.syntax.id` (response) | Viewer → Extension | Response     | `{ id: string }`           |
| `language.syntax`               | Extension → Viewer | Call         | `{ kind: string }`         |
| `language.syntax` (response)    | Viewer → Extension | Response     | `LanguageInfo`             |
| `language.syntax.cache`            | Extension → Viewer | Call         | _(no parameters)_                    |
| `language.syntax.cache` (response) | Viewer → Extension | Response     | `SyntaxCacheList`                    |
| `language.syntax.get`              | Extension → Viewer | Call         | `{ filename: string, as_json?: boolean }` |
| `language.syntax.get` (response)   | Viewer → Extension | Response     | `SyntaxCacheFile`                    |
| `language.syntax.change`        | Viewer → Extension | Notification | `SyntaxChange`             |
| `script.compiled`               | Viewer → Extension | Notification | `CompilationResult`        |
| `runtime.debug`                 | Viewer → Extension | Notification | `RuntimeDebug`             |
| `runtime.error`                 | Viewer → Extension | Notification | `RuntimeError`             |

## Session Management Interfaces

### SessionHandshake

**JSON-RPC Method:** `session.handshake` (call from viewer)

The initial handshake call sent by the viewer to establish a session.

```typescript
interface SessionHandshake {
  server_version: "1.0.0";
  protocol_version: "1.0";
  viewer_name: string;
  viewer_version: string;
  agent_id: string;
  agent_name: string;
  challenge?: string;
  languages: string[];
  syntax_id: string;
  features: { [feature: string]: boolean };
}
```

**Fields:**

- `server_version`: Fixed version "1.0.0" indicating the server API version
- `protocol_version`: Fixed version "1.0" for the communication protocol
- `viewer_name`: Name of the Second Life viewer application
- `viewer_version`: Version string of the viewer
- `agent_id`: Unique identifier for the user/agent
- `agent_name`: Human-readable name of the agent
- `challenge` (optional): Path to a temporary file on the local filesystem containing a UUID. The client must read this file and return the UUID as `challenge_response` to authenticate the connection.
- `languages`: Array of supported scripting languages (e.g., `["lsl", "luau"]`)
- `syntax_id`: Current active syntax identifier as a UUID string
- `features`: Dictionary of feature flags indicating viewer capabilities. Known flags:
  - `live_sync`: Viewer supports live script synchronisation with the external editor
  - `compilation`: Viewer will forward compilation results via `script.compiled`
  - `syntax_cache`: Viewer supports `language.syntax.cache` and `language.syntax.get` for retrieving syntax definition files

### SessionHandshakeResponse

**JSON-RPC Method:** Response to `session.handshake`

The response sent by the VS Code extension to complete the handshake.

```typescript
interface SessionHandshakeResponse {
  client_name: string;
  client_version: "1.0";
  protocol_version: string;
  challenge_response?: string;
  languages: string[];
  features: { [feature: string]: boolean };
  script_name?: string;
  script_language?: string;
}
```

**Fields:**

- `client_name`: Name of the client (VS Code extension)
- `client_version`: Fixed version "1.0" of the client
- `protocol_version`: Protocol version the client supports
- `challenge_response` (optional): The UUID read from the temporary file identified by the `challenge` field in the handshake. Must be provided if `challenge` was present, otherwise the connection will be closed.
- `languages`: Array of languages supported by the client
- `features`: Dictionary of features supported by the client
- `script_name` (optional): Name of the script currently open in the editor
- `script_language` (optional): Language of the script currently open in the editor (e.g. `"lsl"`, `"luau"`)

### Session OK

**JSON-RPC Method:** `session.ok` (notification from viewer)

Confirmation notification sent by the viewer after successful handshake completion. No parameters are sent with this notification.

### SessionDisconnect

**JSON-RPC Method:** `session.disconnect` (notification, bidirectional)

Message sent when terminating the connection.

```typescript
interface SessionDisconnect {
  reason: number;
  message: string;
}
```

**Fields:**

- `reason`: Numeric code indicating the reason for disconnection:
  - `0`: Normal closure
  - `1`: Editor closed
  - `2`: Protocol error
  - `3`: Connection timeout
  - `4`: Internal server error
- `message`: Human-readable description of the disconnect reason

## Language and Syntax Interfaces

### SyntaxChange

**JSON-RPC Method:** `language.syntax.change` (notification from viewer)

Notification sent when the active language syntax changes in the viewer.

```typescript
interface SyntaxChange {
  id: string;
}
```

**Fields:**

- `id`: UUID string identifying the new syntax version

### Language Syntax ID Request

**JSON-RPC Method:** `language.syntax.id` (call from extension to viewer)

Requests the current active language syntax identifier from the viewer. This method takes no parameters.

**Response:** Returns `{ id: string }` where `id` is the current syntax version as a UUID string.

### Language Syntax Request

**JSON-RPC Method:** `language.syntax` (call from extension to viewer)

Requests the in-memory keyword definitions for a specific language. These definitions are the deserialized, viewer-processed form of the syntax data for the current region.

**Parameters:**

```typescript
{
  kind: string; // The language whose definitions to retrieve
}
```

**Valid `kind` values:**

| Value | Description |
| ----------- | ----------------------------------------- |
| `"defs.lsl"` | Returns the LSL keyword definitions |
| `"defs.lua"` | Returns the Luau keyword definitions |

**Response:**

```typescript
interface LanguageInfo {
  id: string;
  defs?: object;    // Present only on success
  success: boolean;
  error?: string;   // Present only on failure
}
```

**Response Fields:**

- `id`: The current syntax version identifier
- `defs` (optional): The keyword definitions object. Only present when `success` is `true`. Structure varies by language.
- `success`: Whether the definitions were found and returned successfully
- `error` (optional): Human-readable error description. Only present when `success` is `false`

**Error cases:**

- No `kind` parameter supplied: `success: false`, `error: "No syntax category specified"`
- Unknown `kind` value: `success: false`, `error: "Unknown syntax category requested"`

### Language Syntax Cache List

**JSON-RPC Method:** `language.syntax.cache` (call from extension to viewer)

Requests the list of file names currently held in the `LLSyntaxDefCache`. This provides the extension with the available syntax definition file names that can subsequently be retrieved with `language.syntax.get`. This method takes no parameters.

**Response:**

```typescript
interface SyntaxCacheList {
  files: string[];  // Array of file names (e.g. ["lsl_keywords.xml", "slua_definitions.yaml"])
  success: boolean;
}
```

**Response Fields:**

- `files`: Array of file name strings, each of which can be passed as the `filename` parameter to `language.syntax.get`
- `success`: Whether the request was handled successfully

**Known cache files:**

| File name | Description |
| -------------------------------- | ---------------------------------------------------- |
| `builtins.txt`                   | LSL built-in keyword list in plain text format |
| `lsl_definitions.yaml`           | LSL language definitions in YAML format |
| `lsl_keywords.xml`               | LSL keyword definitions in LLSD XML format. Used by the viewer's script editor |
| `lsl_keywords_pretty.xml`        | LSL keyword definitions in formatted LLSD XML format |
| `secondlife.d.luau`              | Luau type definition file. Used by luau-lsp |
| `secondlife.docs.json`           | Luau documentation data in JSON format. Used by luau-lsp |
| `slua_definitions.yaml`          | Luau language definitions in YAML format |
| `lua_keywords.xml`               | Luau keyword definitions in LLSD XML format. Used by the viewer's script editor |
| `lua_keywords_pretty.xml`        | Luau keyword definitions in formatted LLSD XML format |
| `secondlife_selene.yml`          | Luau Selene linter configuration in YAML format |

Not all files may be present in every cache — the actual list returned by `language.syntax.cache` reflects only what is available on the viewer's local filesystem at the time of the request.

### Language Syntax Cache Get

**JSON-RPC Method:** `language.syntax.get` (call from extension to viewer)

Requests the content of a specific file from the syntax definition cache. The file name must be one of the names returned by a prior `language.syntax.cache` call. Content is returned either as a raw text string or as a parsed JSON/LLSD object depending on the `as_json` parameter.

**Parameters:**

```typescript
{
  filename: string;   // The file name to retrieve, as returned by language.syntax.cache
  as_json?: boolean;  // Optional. If true, content is returned as a parsed object rather than raw text
}
```

**Fields:**

- `filename`: The file name to retrieve (e.g. `"lsl_keywords.xml"`, `"slua_definitions.yaml"`)
- `as_json` (optional): When `true`, the file is deserialized and returned as a structured object in `content`. When omitted or `false`, `content` is the raw text of the file.

**Response:**

```typescript
interface SyntaxCacheFile {
  content?: string | object;  // Present only on success. String if as_json is false/omitted, object if as_json is true
  success: boolean;
  error?: string;             // Present only on failure
}
```

**Response Fields:**

- `content`: The file content. Only present when `success` is `true`. Is a raw text string when `as_json` is omitted or `false`; is a parsed object when `as_json` is `true`.
- `success`: Whether the file was found and read successfully
- `error` (optional): Human-readable error description. Only present when `success` is `false`

**Error cases:**

- No `filename` parameter supplied: `success: false`, `error: "No filename specified"`
- Name not found in cache: `success: false`, `error: "Requested syntax cache file not found"`
- File could not be loaded: `success: false`, `error: "Failed to load syntax cache file"` (or `"Failed to load and format syntax cache file."` when `as_json` is `true`)

## Script Subscription Interfaces

### ScriptSubscribe

**JSON-RPC Method:** `script.subscribe` (call from extension to viewer)

Requests subscription to a script for live synchronization between the editor and viewer.

```typescript
interface ScriptSubscribe {
  script_id: string;
  script_name: string;
  script_language: string;
}
```

**Fields:**

- `script_id`: Unique identifier for the script to subscribe to
- `script_name`: Display name of the script file
- `script_language`: Programming language of the script (e.g., "lsl", "luau")

### ScriptSubscribeResponse

**JSON-RPC Method:** Response to `script.subscribe`

Response from the viewer indicating whether script subscription was successful.

```typescript
interface ScriptSubscribeResponse {
  script_id: string;
  success: boolean;
  status: number;
  object_id?: string;
  item_id?: string;
  message?: string;
}
```

**Fields:**

- `script_id`: The script identifier that was subscribed to
- `success`: Whether the subscription was successful
- `status`: Numeric status code indicating the result:
  - `0`: Success
  - `1`: Invalid editor — the script editor panel is no longer open
  - `2`: Invalid subscription — no subscription found for the given `script_id`
  - `3`: Already subscribed — another connection is already subscribed to this script
  - `4`: Internal server error
- `object_id` (optional): The in-world UUID of the object containing the script
- `item_id` (optional): The inventory item UUID of the script within the object
- `message` (optional): Additional information about the subscription result

### ScriptUnsubscribe

**JSON-RPC Method:** `script.unsubscribe` (notification from viewer)

Notification sent by the viewer when a script subscription should be terminated.

```typescript
interface ScriptUnsubscribe {
  script_id: string;
}
```

**Fields:**

- `script_id`: Unique identifier for the script to unsubscribe from

### ScriptList

**JSON-RPC Method:** `script.list` (call from extension to viewer)

Requests the list of all scripts currently open and tracked by the viewer, along with the viewer's temp directory. This is intended for use by a file watcher tool that needs to discover which script temp files are active without going through the full `script.subscribe` flow. This method takes no parameters.

**Response:**

```typescript
interface ScriptList {
  temp_dir: string;
  script_ids: string[];
  success: boolean;
}
```

**Response Fields:**

- `temp_dir`: The absolute path to the viewer's temp directory where live-sync script files are written. Combined with a `script_id`, the caller can locate the corresponding temp file on disk.
- `script_ids`: Array of script ID strings for all currently subscribed scripts, across all active connections.
- `success`: Always `true`.

## Compilation Interfaces

### CompilationError

Individual compilation error record.

```typescript
interface CompilationError {
  row: number;
  column: number;
  level: string;
  message: string;
  format?: "lsl";  // Present only for LSL compilation errors
}
```

**Fields:**

- `row`: Line number where the error occurred (1-based for both LSL and Luau)
- `column`: Column position of the error (1-based for LSL; always `0` for Luau as the compiler does not provide column information)
- `level`: Compiler severity string (e.g. `"ERROR"`, `"WARNING"`)
- `message`: Error description
- `format` (optional): Present and set to `"lsl"` for LSL compilation errors; absent for Luau errors

### CompilationResult

**JSON-RPC Method:** `script.compiled` (notification from viewer)

Result of a compilation operation in the viewer.

```typescript
interface CompilationResult {
  script_id: string;
  success: boolean;
  running: boolean;
  errors?: CompilationError[];
}
```

**Fields:**

- `script_id`: Unique identifier for the script that was compiled
- `success`: Whether the compilation was successful
- `running`: Whether the compiled script is currently running
- `errors` (optional): Array of compilation errors if any occurred

## Runtime Event Interfaces

### RuntimeDebug

**JSON-RPC Method:** `runtime.debug` (notification from viewer)

Debug message notification sent by the viewer during script execution.

```typescript
interface RuntimeDebug {
  script_id: string;
  object_id: string;
  object_name: string;
  message: string;
}
```

**Fields:**

- `script_id`: Unique identifier for the script generating the debug message
- `object_id`: Unique identifier for the object containing the script
- `object_name`: Human-readable name of the object
- `message`: The debug message content

### RuntimeError

**JSON-RPC Method:** `runtime.error` (notification from viewer)

Runtime error notification sent by the viewer when a script encounters an error during execution.

```typescript
interface RuntimeError {
  script_id: string;
  object_id: string;
  object_name: string;
  message: string;
  error: string;
  line: number;
  stack?: string[];
}
```

**Fields:**

- `script_id`: Unique identifier for the script that encountered the error
- `object_id`: Unique identifier for the object containing the script
- `object_name`: Human-readable name of the object
- `message`: The full raw chat text of the runtime error message as received from the simulator
- `error`: Extracted error description. Currently always an empty string — runtime error extraction from the simulator's multi-message format is not yet fully implemented.
- `line`: Line number where the error occurred. Currently always `0` for the same reason.
- `stack` (optional): Stack trace lines if they could be extracted from the error message

## Handler and Configuration Interfaces

### WebSocketHandlers

Event handler interface for WebSocket events.

```typescript
interface WebSocketHandlers {
  onHandshake?: (message: SessionHandshake) => SessionHandshakeResponse;
  onHandshakeOk?: () => void;
  onDisconnect?: (message: SessionDisconnect) => void;
  onSubscribe?: (message: ScriptSubscribe) => ScriptSubscribeResponse;
  onUnsubscribe?: (message: ScriptUnsubscribe) => void;
  onSyntaxChange?: (message: SyntaxChange) => void;
  onConnectionClosed?: () => void;
  onCompilationResult?: (message: CompilationResult) => void;
  onRuntimeDebug?: (message: RuntimeDebug) => void;
  onRuntimeError?: (message: RuntimeError) => void;
}
```

**Methods:**

- `onHandshake`: Handler for initial handshake message, returns handshake response
- `onHandshakeOk`: Handler called when handshake is successfully completed
- `onDisconnect`: Handler for disconnect notifications
- `onSubscribe`: Handler called when the extension sends a `script.subscribe` request, returns subscription response
- `onUnsubscribe`: Handler for script unsubscription notifications from viewer
- `onSyntaxChange`: Handler for syntax change notifications
- `onConnectionClosed`: Handler called when connection is closed
- `onCompilationResult`: Handler for compilation result notifications
- `onRuntimeDebug`: Handler for runtime debug message notifications
- `onRuntimeError`: Handler for runtime error notifications

### ClientInfo

Client information used in handshake responses.

```typescript
interface ClientInfo {
  scriptName: string;
  scriptId: string;
  extension: string;
}
```

**Fields:**

- `scriptName`: Name of the script being edited
- `scriptId`: Unique identifier for the script
- `extension`: File extension or script type

