# Dying Light config patcher

A utility that accepts a list of modifications (using custom syntax) to apply to Dying Light's config files.
Accepts a folder of modification instruction files (raw text), and automatically unpacks, edits, and repacks the game's config files.
Uses libzippp for unpacking and repacking.

## Motivations

- Every time Dying Light releases a patch, it overwrites any user modification to its config files
- Users have to re-do their changes after every patch, which is usually just busywork
- Often, user changes and dev changes on a single file can coexist

Hence this utility. Users, instead of having to re-do all their mods after every patch, can now write diff files for this util to parse, using a custom syntax.
In these diff files users can target specific script functions, script variables and scripts imports/exports.
All of the above can be changed (renamed/redefined) or deleted, and new ones can be inserted.

Specific syntax details of the custom modlist syntax to follow.
