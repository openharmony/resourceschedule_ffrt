# Contributing Guidelines

## Contribute

We welcome contributions from the community! By contributing, you can help improve FFRT and make it more robust.

Here are some ways you can contribute:

1. Report Issues:

    - If you encounter any bugs or have suggestions for improvements, please open an issue on this repository. Provide detailed information to help us understand and resolve the issue.

2. Submit Pull Requests:

    - If you have made improvements to the code, feel free to submit a pull request. Ensure that your code adheres to the projects's coding standards and is well-documented.
    - Please ensure that your code follows the project's coding standards. We use `.clang-format` to maintain consistent code style. Run the following command to format your code before committing:

        ```shell
        clang-format -i file
        ```

    - To ensure documentation quality, we use:

        - `cspell.json` for spell checking. Run the following command to check for spelling errors:

            ```shell
            cspell "src/**/*.cpp" "docs/**/*.md"
            ```

        - `.markdownlint.json` for Markdown file format standards. Run the following commands to lint your Markdown files:

            ```shell
            markdownlint "**/*.md"
            ```

        - `.autocorrectrc` and `.autocorrectignore` for ensuring proper formatting of mixed Chinese and English content. Run the following commands to autocorrect documentation:

            ```shell
            autocorrect "**/*.md"
            ```

    - We also recommend using the following VSCode plugins to help adhere to coding and documentation standards:

        - [Clang-Format](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format) or [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) for maintaining consistent code style.
        - [Code Spell Checker](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker) for spell checking
        - [markdownlint](https://marketplace.visualstudio.com/items?itemName=DavidAnson.vscode-markdownlint) for Markdown file format linting.
        - [AutoCorrect](https://marketplace.visualstudio.com/items?itemName=huacnlee.autocorrect) for auto-correcting mixed Chinese and English content.

3. Write Tests:

    - To maintain the quality of the project, we encourage you to write tests for new features and bug fixes. This helps ensure that the codebase remains stable and reliable.

4. Improve Documentation:

    - Good documentation is key to a successful project. If you find areas in the documentation that can be improved, please contribute by updating the `README.md` or other documentation files.

5. Review Pull Requests:

    - Reviewing and providing feedback on other contributor's pull requests is a valuable way to contribute. Your insights can help maintain the quality and consistency of the project.

## Header File Doxygen Conventions

FFRT header files use Doxygen comments to generate the public API reference. Follow these rules when adding a new interface, and apply the same rules to normalize comments that have drifted.

### 1. Module description (`@addtogroup FFRT` block)

- Spell out the full name: `Function Flow Runtime (FFRT) C APIs`, `C++ APIs`, or `C and C++ APIs` (the last only for the umbrella header). Never abbreviate to "FFRT" alone.
- Append one descriptive sentence: `FFRT is a task-based concurrent runtime library that automatically schedules tasks according to their dependencies, eliminating the need for manual thread management.`
- Add `@since` matching the version when the FFRT runtime was first exposed in the SDK (`10`, `12`, `18`, or `20`).

### 2. File header (`@file` block)

- `Declares the {list of interfaces} in {C|C++}.`
- Reference real identifiers with `{@link name}`.
  - For C headers, link to C names (e.g. `{@link ffrt_mutex_init}`).
  - For C++ headers, link to C++ names in the `ffrt::` namespace (e.g. `{@link ffrt::mutex}`). Never link to the underlying C type from a C++ header.
- Do not use the `@include` tag — in stock Doxygen it means "embed file contents into the doc block", not "the header path this API lives in". Use `{@link}` for cross-references instead.
- Always include the three meta tags in this order: `@library libffrt.z.so`, `@kit FunctionFlowRuntimeKit`, `@syscap SystemCapability.Resourceschedule.Ffrt.Core`.
- Add `@since` matching the `@addtogroup` block.

### 3. Function and method briefs

- Use third-person singular verb form: `Initializes a mutex`, `Gets the ID`, `Submits a task`. Never the bare infinitive (`Initialize`, `Get`) and never the imperative.
- Avoid a redundant `FFRT` prefix in briefs describing FFRT types/functions — the module context (`@addtogroup FFRT`) already establishes the namespace. Prefer `Defines the mutex structure.` over `Defines the FFRT mutex structure.`
- Use `concurrent` (not `parallel`) when describing FFRT's concurrency model — this matches the public API names (e.g. `ffrt_queue_concurrent`, `ffrt_function_kind_general` vs `_queue`).
- For multi-sentence briefs, the first line is the short summary; leave a blank line; then the detail paragraph.
- For class, struct, and enum declarations, the first line is a short summary followed by a detail paragraph.
- Use precise vocabulary. Prefer `Allocates memory` over `Applies memory`; prefer `scheduled before` over `prior to`.
- For destroy / dispose / release functions, omit boilerplate like "the user needs to invoke this interface" — the brief alone is enough.
- For wrappers around standard library primitives (mutex, condition variable, etc.), document the standard contract (e.g. the mutex must be held on entry, atomically released, re-acquired on wakeup, predicate re-checked).

### 4. Cross-references and emphasis

- Use backticks for inline code identifiers. Prefer backticks over the legacy `@c <identifier>` syntax — backticks render identically in English and Chinese source files, and the visible delimiters help IDE parsers handle them correctly even adjacent to CJK punctuation.
- Use `{@link name}` for cross-references to other APIs (renders as a clickable link).
- Use `{@see name}` in a dedicated tag for "see also" references.
- Do not add `@see` for symmetric lifecycle pairs where the relationship is obvious from the function name or body — `init` ↔ `destroy`, `create` ↔ `destroy`, `lock` ↔ `unlock`, `signal` ↔ `broadcast`, `get` ↔ `set`, `get` ↔ `update` inverse, `submit` → `alloc_auto_managed` inverse. Reserve `@see` for cross-domain relationships.
- Never use `<b>...</b>` for emphasis. Use backticks instead.
- Do not use the `@warning` or `@note` tags. Promote any warning or note into the surrounding prose as a regular sentence.

### 5. Parameter descriptions (`@param`)

- Begin every description with `Indicates`.
- Pointer params: ``Indicates a pointer to the X.``
- Value or reference params: ``Indicates the X.``
- Enum params: list the valid values, e.g. ``Indicates the X, which can be `value_a` or `value_b`.``
- Do not repeat a constraint already stated in `@param` (unit, lower/upper limit, etc.) in the `@brief` or detail paragraph. State each fact once, in the most specific tag.

### 6. Return descriptions (`@return`)

- For `void` returns, omit `@return` entirely. Reserve `@return` for non-void returns.
- Booleans: `` `true` if X; `false` otherwise. ``
- Error codes: `` `ffrt_success` if X; `ffrt_error_inval` otherwise. `` (backticks around the symbol).
- Zero-or-minus-one returns: `` `0` if X; `-1` otherwise. ``
- Non-null returns: `A non-null X if …; a null pointer otherwise.`
- When the return type already makes the value obvious, drop redundant words from the description. E.g. `submit_h` returns a task handle by signature, so the brief can say `obtains a handle` rather than `obtains a task handle`.

### 7. `@since` semantics

- Add `@since` to every interface that is part of the SDK.
- For C++ wrappers, `@since` must match the underlying C function's `@since` (if the C function is in the SDK, the C++ wrapper is too).
- C and C++ interfaces unique to the open-source repo (not in the SDK) carry no `@since`.
- When an interface was added in a later version, add only the new-version `@since`. Do not accumulate.

### 8. Type tags

- `@class` for C++ classes.
- `@struct` for C structs and C++ structs.
- `@enum` for C enums and C++ enums.
- `@namespace` for C++ namespaces.
- `@file` for the file-level block.
- Multi-line enum member comments must use an explicit `@brief`, even when the comment carries no detail paragraph and the multi-line shape exists only to host `@since`. Single-line `/** … */` comments need no `@brief`.
- `@brief` must occupy its own line in multi-line Doxygen comments, aligned with `@since` and other tags. The `/** @brief …` form (where `@brief` is jammed onto the same line as `/**`) is reserved for single-line comments only — never use it for multi-line blocks.
- For enum boundary values that exist only as a marker, document them as `sentinel` (e.g. `ffrt_queue_max` is a sentinel for iteration).
- For enums with inline enumerator comments, do not restate those comments in the surrounding `@enum` `@brief` or detail paragraph. Document the *relationships* and ordering, not each value.

### 9. Layout

- Wrap lines at 120 characters.
- Leave a blank line between the brief and the detail paragraph.
- Inside a Doxygen block, continuation/detail lines use a single space after `*` (e.g. ` * text`), aligned with the opening `/**`. Do not use two spaces (` *  text`) — that form drifts out of sync with `@param`/`@return` lines and breaks column alignment.
- Multi-line Doxygen blocks for top-level declarations (enum members, struct/typedef declarations, function declarations) must start with `/**` on its own line and put the explicit `@brief` tag on the next line. The wrapped-line form — `/** first-sentence` immediately followed by ` * continuation` on the next line — is forbidden for these declarations: it conflates brief and detail into one paragraph, defeats the brief/detail split required by §3, and breaks column alignment with `@param`/`@return` lines in the same block.
- Struct field comments should not use `@brief`. Prefer the single-line form when the comment fits; otherwise put the opening alone on the first line and continue the text on subsequent lines as a single paragraph — no blank separator, no `@brief` tag.
- Close the file's `@addtogroup` block with `/** @} */` at the end of the file.
- Leave a blank line between the file header's closing `*/` and the `#ifndef` include guard.

### 10. C++-specific

- Method briefs in C++ classes are typically a single short line; C function briefs are usually brief plus a detail paragraph.
- Use `Gets` for getter methods. Not `Get`, not `Obtains`.
- Always reference the C++ type in `{@link}` from a C++ header, never the underlying C type.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](https://developer.huawei.com/consumer/en/devservice/guidelines/). Please read it to understand the standards and expectations for contributing.

## Get in Touch

If you have any questions or need further guidance, feel free to reach us via [email](mailto:hiffrt@huawei.com) or join our [Q&A community](https://developer.huawei.com/consumer/cn/forum/).

Thank you for your contributions and support!
