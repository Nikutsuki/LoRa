# Agent Guidelines

Clean code is the main goal for this repository. Prefer readable, maintainable implementations over quick fixes or tightly coupled code.

## Code Structure

- Keep code modular and split responsibilities into focused, reusable classes.
- Prefer small, cohesive classes with clear public APIs.
- Avoid large monolithic files, duplicated logic, and hidden dependencies.
- Place shared behavior behind reusable abstractions when it improves clarity.
- Keep implementation details encapsulated so modules can be tested and reused independently.

## Clean Code Expectations

- Use clear names for classes, functions, variables, and files.
- Keep functions short and focused on one responsibility.
- Make control flow easy to follow.
- Add comments only when they explain non-obvious intent or constraints.
- Favor explicit, simple code over clever shortcuts.
- Preserve existing project conventions unless there is a strong reason to change them.

## Changes

When modifying this codebase, aim for changes that make the design easier to understand, extend, test, and maintain.
