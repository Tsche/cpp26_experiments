# Notes


## VSCode recognize files with no suffix as C++
Files with no extension aren't recognized as C++ by default. To fix this add the following to .vscode/settings.json in the repository root:

```json
"files.associations": {
  "**/include/erl/{[!.],[!.][!.],[!.][!.][!.],[!.][!.][!.][!.],[!.][!.][!.][!.][!.]}{[],[!.],[!.][!.][!.][!.][!.],[!.][!.][!.][!.][!.][!.][!.][!.][!.][!.]}": "cpp"
}
```