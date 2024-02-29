
# Build a mini DBMS
A simple DBMS written in C on Ubuntu with a B+ tree implementation

## Purpose

- Basic understanding of how databases work on hard drives.
- Understand search algorithms using database indexes on metadata

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
- .exit - exit and save the data

The application is written in C, data is deployed in B+ tree and saved in data.db file.
We read and flush data into the file through structs Pager. Pager contains an array of pointers to pages as the page is read from the hard drive, each page is 4 kb long and stores the data of a node in tree B.

 


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





## 🚀 About Me

I am a student studying information-analysis system security at MEPhI

