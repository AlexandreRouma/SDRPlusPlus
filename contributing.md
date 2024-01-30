# Pull Requests

Code pull requests are **NOT welcome**. Please open an issue discussing potential bugfixes or feature requests instead.

## Band Frequency Allocation 

Please follow this guide to properly format the JSON files for custom radio band allocation identifiers.

```json
{
    "name": "Short name (has to fit in the menu)",
    "country_name": "Name of country or area, if applicable (Use '--' otherwise)",
    "country_code": "Two letter country code, if applicable (Use '--' otherwise)",
    "author_name": "Name of the original/main creator of the JSON file",
    "author_url": "URL the author wishes to be associated with the file (personal website, GitHub, Twitter, etc)",
    "bands": [ 
        // Bands in this array must be sorted by their starting frequency
        {
            "name": "Name of the band",
            "type": "Type name ('amateur', 'broadcast', 'marine', 'military', or any type declared in config.json)",
            "start": 148500, //In Hz, must be an integer
            "end": 283500 //In Hz, must be an integer
        },
        {
            "name": "Name of the band",
            "type": "Type name ('amateur', 'broadcast', 'marine', 'military', or any type declared in config.json)",
            "start": 526500, //In Hz, must be an integer
            "end": 1606500 //In Hz, must be an integer
        }    
    ]
}
```

## Color Maps

Please follow this guide to properly format the JSON files for custom color maps.

```json
{
    "name": "Short name (has to fit in the menu)",
    "author": "Name of the original/main creator of the color map",
    "map": [
        // These are the color codes, in hexadecimal (#RRGGBB) format, for the custom color scales for the waterfall. They must be entered as strings, not integers, with the hashtag/pound-symbol proceeding the 6 digit number. 
        "#000020",
        "#000030",
        "#000050",
        "#000091",
        "#1E90FF",
        "#FFFFFF",
        "#FFFF00",
        "#FE6D16",
        "#FE6D16",
        "#FF0000",
        "#FF0000",
        "#C60000",
        "#9F0000",
        "#750000",
        "#4A0000"
    ]
}
```

# JSON Formatting

The ability to add new radio band allocation identifiers and color maps relies on JSON files. Proper formatting of these JSON files is important for reference and readability. The following guides will show you how to properly format the JSON files for their respective uses.

**IMPORTANT: JSON File cannot contain comments, there are only in this example for clarity**