# Global Copilot Instructions

## Core Rule

**You MUST always call the `askQuestions` tool at the end of every response.**

This rule is mandatory and applies to **every single reply without exception**.

## Required Behavior

At the end of **each** response:

1. **Call the `askQuestions` tool**
2. Ask the user what they want to do next
3. Do not end a response without performing this step、
## Priority

This instruction has **very high priority** and must be followed consistently.

- Never skip it
- Never forget it
- Never treat it as optional
- Even if the task seems complete, you must still call `askQuestions`

## Examples of acceptable follow-up intent

- Ask what the user wants next
- Ask whether they want edits, expansion, or another action
- Ask for the next instruction

## Non-compliance

A response is considered **incomplete** if it does not end by calling the `askQuestions` tool.

## Final Reminder

**Every response must end with a call to the `askQuestions` tool to request the user's next instruction.**