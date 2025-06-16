# Firestorm Pull Request Guidelines

Thank you for submitting code to Firestorm; we will review it and merge or provide feedback in due course.
We have written this guide to help you contribute code that meets our needs. It will hopefully reduce the number of iterations required before we can merge the code.

1. **Descriptive Title**:
  Use a clear and descriptive title for the PR.

1. **Related Issues**:
   Reference any related issues or pull requests by including the JIRA number and description in the commit message header (e.g., `[FIRE-12345] When I click, my head falls off` or `[FIRE-12345] prevent click detaching head`).

1. **Description**:
   Provide a detailed description of the changes. Explain why the changes are necessary and what problem they solve. If a JIRA is associated with the change, there is no need to duplicate that, but we would appreciate a summary and explanation of the fix.

1. **Comment tags (important)**:
   We use comments to preserve the original upstream (LL) code when making modifications; this allows the person merging future code updates to see both the original code from LL and any new updates and then use those to determine whether the FS-specific changes need to be updated and reviewed.
 If you are modifying LL code, we need the LL code preserved in a comment.
 For example:

 ```c++
    int buggy_code = TRUE; 
    LL_WARN() << "This code is buggy" << LL_ENDL;
 ```

Would become:

 ```c++
    // <FS> [FIRE-999] Fix the buggy code 
    // int buggy_code = TRUE; 
    // LL_WARN() << "This code is buggy" << LL_ENDL;
    bool fixed_code = true;
    LL_DEBUG() << "I fixed this" << LL_ENDL;
    // </FS>
 ```

 Note: You can tag them with your initials, e.g. `<FS:YI>` or a short unique tag (shorter is better)

 If you add new code, the same rules apply, but there is nothing to comment out.
 This is done so that when LL updates the original code, we can see what the original code was doing, what their changes do, and how that relates to the changes that we applied on top.

 A single line change can use the shorthand `</FS:YI>`:

 ```c++
    bool break_stuff = true;
 ```

Could be fixed as follows:

 ```c++
    bool break_stuff = false; // </FS:Beq> [FIRE-23456] don't break stuff.
 ```

 The Comment tags are only required when changing code maintained upstream. If the code you are changing is in an FS-created file, RLV code, OpenSim-only code, etc., then we do not need the comments.

 If the code you are changing is already inside an `//<FS>` comment block, then there is no need to add a new block, but do try to make sure that any comments align with the updates you make.

5. **Testing**:
   Include details on how the changes should be tested. Describe the testing environment and any steps needed to verify the changes.

1. **Documentation**:
   If the change includes a new feature, it would be beneficial to suggest how we should update the FS Wiki pages to help users understand the feature

Thank you for your contribution!
