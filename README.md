# Installation and Build Instructions
**Pull with Git LFS**:
    ```sh
    git lfs pull
    ```
1. **Install `make`**:
    ```sh
    sudo apt-get install make
    ```

2. **Navigate to the source directory**:
    ```sh
    cd search_system/src
    ```

3. **Build the project**:
    ```sh
    make
    ```

4. **Clean up old builds before recompiling**:
    ```sh
    make clean
    ```

5. **Run the parser and indexer**:
    ```sh
    ../build/parser_and_indexer
    ```

5. **Run the merger**:
    ```sh
    ../build/merger
    ```


    