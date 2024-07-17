## Firestorm Pull Request Guidelines

Thank you for submitting code to Firestorm, we will review it and merge or provide feedback in due course. 
To reduce the number of iterations that we request from you it is useful to ensure your PR meets the following guidelines:

1. **Descriptive Title**:
   - Use a clear and descriptive title for the PR.

2. **Related Issues**:
   - Reference any related issues or pull requests by including the JIRA number and description in the commit message header (e.g., `[FIRE-12345] When I click, my head falls off` or `[FIRE-12345] prevent click detaching head`).

3. **Description**:
   - Provide a detailed description of the changes. Explain why the changes are necessary and what problem they solve. If there is a JIRA associated then there is no need to duplicate that, but a short summary and explanation of the fix woul dbe appreciated.

4. **Comment tags (important)**:
   - We use comments to preserve the original upstream (LL) code when making modifications. Thsi allows the person merging future code updates to see both the original code from LL, and their updates and use those to determine whether the FS specific changes need to be updated and reviewed.
   If you are modifying LL code, we need the LL code preserved in a comment.
    For example:
    ```c++
    int buggy_code = TRUE; 
    LL_WARN() << "This code is buggy" << LL_ENDL;
    ```
    would become:
    ```c++
    // <FS> [FIRE-999] Fix the buggy code 
    // int buggy_code = TRUE; 
    // LL_WARN() << "This code is buggy" << LL_ENDL;
    bool fixed_code = true;
    LL_DEBUG() << "I fixed this" << LL_ENDL;
    // </FS>
    ```
    Note: You can tag them with your initials e.g. `<FS:YI>` or a short unique tag (shorter is better)

    If you are simply adding new code then the same rules apply but there is nothing to comment out.
    This is done so that when LL update the original code we can see what the original code was doing, what their changes do and how that relates to the changes that we applied on top.

    A single line change, can use the shorthand `<FS:YI/>`:
    ```c++
    bool break_stuff=true;
    ```
    could be fixed as follows:
    ```c++
    bool break_stuff=false; // <FS:Beq/> [FIRE-23456] don't break stuff.
    ```

4. **Testing**:
   - Include details on how the changes were tested. Describe the testing environment and any steps needed to verify the changes.

5. **Documentation**:
   - If the change includes a new feature it would be really helpful to suggest how the FS Wiki pages might be updated to help users understand the feature

Thank you for your contribution!
