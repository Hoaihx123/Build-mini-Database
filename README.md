
# Build a mini DBMS
A simple DBMS written in C on Ubuntu with a B+ tree implementation.

## Purpose

- Basic understanding of how databases work on hard drives.
- Understand search algorithms using database indexes on metadata.

## Description

So, In this application, I only deploy a database consisting of 1 table and basic sql statements:
| field             | type                                                                |
| ----------------- | ------------------------------------------------------------------ |
| id | uint32_t |
| username | char[32] |
| email | char[256] |

- select - this statement returns the entire table
- insert `key` `username` `email` - search for a position in the tree and insert a row into the table
- select id=`id` - search for lines by id
- delete id=`id` - delete row by id
- update set user_name=`new_username` email=`new_email` where id=`id` - update username or email (or both) by id
- .exit - exit and save the data.

The application is written in C, data is deployed in B+ tree and saved in data.db file.
We read and flush data into the file through structs Pager. Pager contains an array of pointers to pages as the page is read from the hard drive, each page is 4 kb long and stores the data of a node in B-tree.
 


## B+ Tree on disk
In each B-tree there are 2 types of nodes:
- Leaf node
- Internal node
  
At the beginning of each common node, we use byte 0 to store the node's format, byte 1 to store the boolean value is_root, and the next 4 bytes the address of the parent page.

For leaf nodes, use bytes 6-9 (the next 4 bytes) to store the number of rows in the page. Then it will save rows containing keys and values ​​in turn.

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/65adb530-98ec-48b3-ab47-2bfb8e97f4bb)

For internal nodes, use bytes 6-9 (the next 4 bytes) to store the row number in the key, the next 4 bytes (10-13) store the position of the rightmost node.
Then, we will save pairs including the key and the location of the child node in turn.

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/ffae7fe8-4ba6-410b-a984-4c142342e446)


## Demo
First, run the following commands to compile the code:
```bash
  gcc -c inputBuffer/inputBuffer.c

  gcc inputBuffer/inputBuffer.c miniDB.c
```

Initially our table has nothing:

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/d011b428-14cf-4c38-a50b-cb403de693d4)

So now we start inserting new rows.

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/16a7d4ca-8ae5-446b-aeae-35659978addf)

Try **`select`** command:

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/fbfe1fdd-7438-4860-8b9d-feb4c4ace9ed)

But so that after restarting the program the next time the data is still there, we need run the **`.exit`** command.

Now I will look at my **`data.db`** file using the **`hexdump`** command, each number in picture corresponds to 4 bits, so 2 consecutive numbers will be 1 byte.

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/e28fae6c-8c3f-41a9-9f2c-71c832e6aa1e)

- Let's look at the first byte: **`01`** - which is the byte containing the **is_root**.
- The next byte is **`00`** - this value means the node under considerations is a `leaf node`.
- The next 4 bytes are the page number of the parent page - **`0000 0000`**.
- The next 4 bytes **`0005 0000`** are the number of rows in this node, clearly we just inserted 5 rows.
- The next 4 bytes **`0001 0000`** are the key of the first row.
- The next bytes are the value of this row ...
To make it easier to test, I changed the LEAF_NODE_MAX_CELLS constant to 5, now we have 5 rows. So let's go back to the program and add a new row to see how things go!

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/f331d2d6-82ba-485b-824b-9fd708f98764)

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/122adebb-aa51-4c0d-bec4-e0e886078c9f)

Our page 0 is still the root but has become an internal node, it contains a key which a value 3, the right child node is contained on page 1, the left child node is on page 2.

Okay, now let's try the other functions!
- **Search by id function:**

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/da2406ea-eb36-4e7e-bfeb-7b211d0775a9)

- **Delete row:**

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/67cdbd6e-7301-4a92-842f-c34e87fe2c3e)
- **Update row:**

![image](https://github.com/Hoaihx123/Build-mini-Database/assets/99666261/4df81fdf-f0ef-40e3-b773-f045ab4f9100)


## 🚀 About Me

My name is Hoai, I am a student studying information-analysis system security at MEPhI.

