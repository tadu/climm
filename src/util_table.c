
#include "micq.h"
#include "util_table.h"
#include "util_ui.h"
#include <assert.h>

static const char *TableMonths[] = {   /* *INDENT-OFF* */
    _i18n (14, "None"),
    _i18n (15, "Jan"),    _i18n (16, "Feb"),    _i18n (17, "Mar"),    _i18n (18, "Apr"),
    _i18n (19, "May"),    _i18n (20, "Jun"),    _i18n (21, "Jul"),    _i18n (22, "Aug"),
    _i18n (23, "Sep"),    _i18n (24, "Oct"),    _i18n (25, "Nov"),    _i18n (26, "Dec")
};

static const char *TableLang[] = {
    _i18n (100, "None"),
    _i18n (101, "Arabic"),      _i18n (102, "Bhojpuri"),    _i18n (103, "Bulgarian"),       _i18n (104, "Burmese"),
    _i18n (105, "Cantonese"),   _i18n (106, "Catalan"),     _i18n (107, "Chinese"),         _i18n (108, "Croatian"),
    _i18n (109, "Czech"),       _i18n (110, "Danish"),      _i18n (111, "Dutch"),           _i18n (112, "English"),
    _i18n (113, "Esperanto"),   _i18n (114, "Estonian"),    _i18n (115, "Farsi"),           _i18n (116, "Finnish"),
    _i18n (117, "French"),      _i18n (118, "Gaelic"),      _i18n (119, "German"),          _i18n (120, "Greek"),
    _i18n (121, "Hebrew"),      _i18n (122, "Hindi"),       _i18n (123, "Hungarian"),       _i18n (124, "Icelandic"),
    _i18n (125, "Indonesian"),  _i18n (126, "Italian"),     _i18n (127, "Japanese"),        _i18n (128, "Khmer"),
    _i18n (129, "Korean"),      _i18n (130, "Lao"),         _i18n (131, "Latvian"),         _i18n (132, "Lithuanian"),
    _i18n (133, "Malay"),       _i18n (134, "Norwegian"),   _i18n (135, "Polish"),          _i18n (136, "Portuguese"),
    _i18n (137, "Romanian"),    _i18n (138, "Russian"),     _i18n (139, "Serbo-Croatian"),  _i18n (140, "Slovak"),
    _i18n (141, "Slovenian"),   _i18n (142, "Somali"),      _i18n (143, "Spanish"),         _i18n (144, "Swahili"),
    _i18n (145, "Swedish"),     _i18n (146, "Tagalog"),     _i18n (147, "Tartar"),          _i18n (148, "Thai"),
    _i18n (149, "Turkish"),     _i18n (150, "Ukrainian"),   _i18n (151, "Urdu"),            _i18n (152, "Vietnamese"),
    _i18n (153, "Yiddish"),     _i18n (154, "Yoruba"),      _i18n (155, "Afrikaans"),       _i18n (156, "Bosnian"),
    _i18n (157, "Persian"),     _i18n (158, "Albanian"),    _i18n (159, "Armenian")
};
#define TableLangSize sizeof (TableLang) / sizeof (const char *)

static const char *TableCountry[] = {
    _i18n (200, "Not entered"),
    _i18n (201, "Afghanistan"), _i18n (202, "Albania"),     _i18n (203, "Algeria"),     _i18n (204, "American Samoa"),
    _i18n (205, "Andorra"),     _i18n (206, "Angola"),      _i18n (207, "Anguilla"),    _i18n (208, "Antigua"),
    _i18n (209, "Argentina"),   _i18n (210, "Armenia"),     _i18n (211, "Aruba"),       _i18n (212, "Ascention Island"),
    _i18n (213, "Australia"),   _i18n (214, "Australian Antartic Territory"),           _i18n (215, "Austria"),
    _i18n (216, "Azerbaijan"),  _i18n (217, "Bahamas"),     _i18n (218, "Bahrain"),     _i18n (219, "Bangladesh"),
    _i18n (220, "Barbados"),    _i18n (221, "Belarus"),     _i18n (222, "Belgium"),     _i18n (223, "Belize"),
    _i18n (224, "Benin"),       _i18n (225, "Bermuda"),     _i18n (226, "Bhutan"),      _i18n (227, "Bolivia"),
    _i18n (228, "Bosnia & Herzegovina"),                    _i18n (229, "Botswana"),    _i18n (230, "Brazil"),
    _i18n (231, "British Virgin Islands"),                  _i18n (232, "Brunei"),      _i18n (233, "Bulgaria"),
    _i18n (234, "Burkina Faso"),_i18n (235, "Burundi"),     _i18n (236, "Cambodia"),    _i18n (237, "Cameroon"),
    _i18n (238, "Canada"),      _i18n (239, "Cape Verde Islands"),                      _i18n (240, "Cayman Islands"),
    _i18n (241, "Central African Republic"),                _i18n (242, "Chad"),        _i18n (243, "Christmas Island"),
    _i18n (244, "Cocos-Keeling Islands"),                   _i18n (245, "Comoros"),     _i18n (246, "Congo"),
    _i18n (247, "Cook Islands"),_i18n (248, "Chile"),       _i18n (249, "China"),       _i18n (250, "Columbia"),
    _i18n (251, "Costa Rice"),  _i18n (252, "Croatia"),     _i18n (253, "Cuba"),        _i18n (254, "Cyprus"),
    _i18n (255, "Czech Republic"),                          _i18n (256, "Denmark"),     _i18n (257, "Diego Garcia"),
    _i18n (258, "Djibouti"),    _i18n (259, "Dominica"),    _i18n (260, "Dominican Republic"),
    _i18n (261, "Ecuador"),     _i18n (262, "Egypt"),       _i18n (263, "El Salvador"), _i18n (264, "Equitorial Guinea"),
    _i18n (265, "Eritrea"),     _i18n (266, "Estonia"),     _i18n (267, "Ethiopia"),    _i18n (268, "F.Y.R.O.M. (Former Yugoslavia)"),
    _i18n (269, "Faeroe Islands"),                          _i18n (270, "Falkland Islands"),
    _i18n (271, "Federated States of Micronesia"),          _i18n (272, "Fiji"),        _i18n (273, "Finland"),
    _i18n (274, "France"),      _i18n (275, "French Antilles"),                         _i18n (276, "French Antilles"),
    _i18n (277, "French Guiana"),                           _i18n (278, "French Polynesia"),
    _i18n (279, "Gabon"),       _i18n (280, "Gambia"),      _i18n (281, "Georgia"),     _i18n (282, "Germany"),
    _i18n (283, "Ghana"),       _i18n (284, "Gibraltar"),   _i18n (285, "Greece"),      _i18n (286, "Greenland"),
    _i18n (287, "Grenada"),     _i18n (288, "Guadeloupe"),  _i18n (289, "Guam"),        _i18n (290, "Guantanomo Bay"),
    _i18n (291, "Guatemala"),   _i18n (292, "Guinea"),      _i18n (293, "Guinea-Bissau"),
    _i18n (294, "Guyana"),      _i18n (295, "Haiti"),       _i18n (296, "Honduras"),    _i18n (297, "Hong Kong"),
    _i18n (298, "Hungary"),     _i18n (299, "Iceland"),     _i18n (300, "India"),       _i18n (301, "Indonesia"),
    _i18n (302, "INMARSAT"),    _i18n (303, "INMARSAT Atlantic-East"),                  _i18n (304, "Iran"),
    _i18n (305, "Iraq"),        _i18n (306, "Ireland"),     _i18n (307, "Israel"),      _i18n (308, "Italy"),
    _i18n (309, "Ivory Coast"), _i18n (310, "Japan"),       _i18n (311, "Jordan"),      _i18n (312, "Kazakhstan"),
    _i18n (313, "Kenya"),       _i18n (314, "South Korea"), _i18n (315, "Kuwait"),      _i18n (316, "Liberia"),
    _i18n (317, "Libya"),       _i18n (318, "Liechtenstein"),                           _i18n (319, "Luxembourg"),
    _i18n (320, "Malawi"),      _i18n (321, "Malaysia"),    _i18n (322, "Mali"),        _i18n (323, "Malta"),
    _i18n (324, "Mexico"),      _i18n (325, "Monaco"),      _i18n (326, "Morocco"),     _i18n (327, "Namibia"),
    _i18n (328, "Nepal"),       _i18n (329, "Netherlands"), _i18n (330, "Netherlands Antilles"),
    _i18n (331, "New Caledonia"),                           _i18n (332, "New Zealand"), _i18n (333, "Nicaragua"),
    _i18n (334, "Nigeria"),     _i18n (335, "Norway"),      _i18n (336, "Oman"),        _i18n (337, "Pakistan"),
    _i18n (338, "Panama"),      _i18n (339, "Papua New Guinea"),                        _i18n (340, "Paraguay"),
    _i18n (341, "Peru"),        _i18n (342, "Philippines"), _i18n (343, "Poland"),      _i18n (344, "Portugal"),
    _i18n (345, "Puerto Rico"), _i18n (346, "Qatar"),       _i18n (347, "Romania"),     _i18n (348, "Russia"),
    _i18n (349, "Saipan"),      _i18n (350, "San Marino"),  _i18n (351, "Saudia Arabia"),
    _i18n (352, "Senegal"),     _i18n (353, "Singapore"),   _i18n (354, "Slovakia"),    _i18n (355, "South Africa"),
    _i18n (356, "Spain"),       _i18n (357, "Sri Lanka"),   _i18n (358, "Suriname"),    _i18n (359, "Sweden"),
    _i18n (360, "Switzerland"), _i18n (361, "Taiwan"),      _i18n (362, "Tanzania"),    _i18n (363, "Thailand"),
    _i18n (364, "Tinian Island"),                           _i18n (365, "Togo"),        _i18n (366, "Tokelau"),
    _i18n (367, "Tonga"),       _i18n (368, "Trinadad and Tabago"),                     _i18n (369, "Tunisia"),
    _i18n (370, "Turkey"),      _i18n (371, "Turkmenistan"),_i18n (372, "Turks and Caicos Islands"),
    _i18n (373, "Tuvalu"),      _i18n (374, "Uganda"),      _i18n (375, "Ukraine"),     _i18n (376, "United Arab Emirates"),
    _i18n (377, "UK"),          _i18n (378, "United States Virgin Islands"),            _i18n (379, "USA"),
    _i18n (380, "Uruguay"),     _i18n (381, "Uzbekistan"),  _i18n (382, "Vanuatu"),     _i18n (383, "Vatican City"),
    _i18n (384, "Venezuela"),   _i18n (385, "Vietnam"),     _i18n (386, "Wallis and Futuna Islands"),
    _i18n (387, "Western Samoa"),                           _i18n (388, "Yemen"),       _i18n (389, "Yugoslavia"),
    _i18n (390, "Zaire"),       _i18n (391, "Zambia"),      _i18n (392, "Zimbabwe"),
    0
};
#define TableCountrySize sizeof (TableCountry) / sizeof (const char *)

static UWORD TableCountryCodes[] = {
    0xffff,
     93,   355,   213,   684,   376,        244,   101,   102,    54,   374,
    297,   274,    61,  6721,    43,        934,   103,   973,   880,   104,
    375,    32,   501,   229,   105,        975,   591,   387,   267,    55,
    106,   673,   359,   226,   257,        855,   237,   107,   238,   108,
    236,   235,   672,  6101,  2691,        242,   682,    56,    86,    57,
    506,   385,    53,   357,    42,         45,   246,   253,   109,   110,
    593,    20,   503,   240,   291,        372,   251,   389,   298,   500,
    691,   679,   358,    33,   596,       5901,   594,   689,   241,   220,
    995,    49,   233,   350,    30,        299,   111,   590,   671,  5399,
    502,   224,   245,   592,   509,        504,   852,    36,   354,    91,
     62,   870,   870,    98,   964,        353,   972,    39,   225,    81,
    962,   705,   254,    82,   965,        231,   218,  4101,   352,   265,
     60,   223,   356,    52,    33,        212,   264,   977,    31,   599,
    687,    64,   505,   234,    47,        968,    92,   507,   675,   595,
     51,    63,    48,   351,   121,        974,    40,     7,   670,    39,
    966,   221,    65,    42,    27,         34,    94,   597,    46,    41,
    886,   255,    66,  6702,   228,        690,   676,   117,   216,    90,
    709,   118,   688,   256,   380,        971,    44,     1,   123,   598,
    711,   678,   379,    58,    84,        681,   685,   967,   381,   243,
    260,  263,
    0
};
#define TableCountryCodesSize sizeof (TableCountryCodes) / sizeof (UWORD)

static const char *TableOccupation[] = {
    _i18n (160, "Not Entered"),
    _i18n (161, "Academic"),                    _i18n (162, "Administrative"),        _i18n (163, "Art/Entertainmant"),
    _i18n (164, "College Student"),             _i18n (165, "Computers"),             _i18n (166, "Community & Social"),
    _i18n (167, "Education"),                   _i18n (168, "Engineering"),           _i18n (169, "Financial Services"),
    _i18n (170, "Government"),                  _i18n (171, "High School Student"),   _i18n (172, "Home"),
    _i18n (173, "ICQ - Providing Help"),        _i18n (174, "Law"),                   _i18n (175, "Managerial"),
    _i18n (176, "Manufacturing"),               _i18n (177, "Medical/Health"),        _i18n (178, "Military"),
    _i18n (179, "Non-government Organization"), _i18n (180, "Professional"),          _i18n (181, "Retail"),
    _i18n (182, "Retired"),                     _i18n (183, "Science & Research"),    _i18n (184, "Sports"),
    _i18n (185, "Technical"),                   _i18n (186, "University Student"),    _i18n (187, "Web Building")
};
#define TableOccupationSize sizeof (TableOccupation) / sizeof (const char *)

static const char *TableInterest[] = {
    _i18n (455, "Art"),                   _i18n (456, "Cars"),                  _i18n (457, "Celebrity Fans"),    
    _i18n (458, "Collections"),           _i18n (459, "Computers"),             _i18n (460, "Culture & Literature"), 
    _i18n (461, "Fitness"),               _i18n (462, "Games"),                 _i18n (463, "Hobbies"),           
    _i18n (464, "ICQ - Providing Help"),  _i18n (465, "Internet"),              _i18n (466, "Lifestyle"),         
    _i18n (467, "Movies/TV"),             _i18n (468, "Music"),                 _i18n (469, "Outdoor Activities"),
    _i18n (470, "Parenting"),             _i18n (471, "Pets/Animals"),          _i18n (472, "Religion"),          
    _i18n (473, "Science/Technology"),    _i18n (474, "Skills"),                _i18n (475, "Sports"),            
    _i18n (476, "Web Design"),            _i18n (477, "Nature and Environment"),_i18n (478, "News & Media"),      
    _i18n (479, "Government"),            _i18n (480, "Business & Economy"),    _i18n (481, "Mystics"),           
    _i18n (482, "Travel"),                _i18n (483, "Astronomy"),             _i18n (484, "Space"),             
    _i18n (485, "Clothing"),              _i18n (486, "Parties"),               _i18n (487, "Women"),             
    _i18n (488, "Social science"),        _i18n (489, "60's"),                  _i18n (490, "70's"),              
    _i18n (491, "80's"),                  _i18n (492, "50's"),              
};
#define TableInterestSize sizeof (TableInterest) / sizeof (const char *)

const char *TableGetMonth (int code)   /* *INDENT-ON* */
{
    if (code > 12)
        code = 0;

    return i18n (-1, TableMonths[code]);
}

const char *TableGetLang (UBYTE code)
{
    if (code >= TableLangSize)
        return i18n (99, "Unknown language.");

    return i18n (-1, TableLang[code]);
}

void TablePrintLang (void)
{
    int i;
    const char *p;

    for (i = 1; i < TableLangSize; i++)
    {
        p = i18n (-1, TableLang[i]);

        M_print ("%2d. %-7s", i, p);
        if ((i + 1) & 3)
            M_print ("\t");
        else
            M_print ("\n");
    }
    M_print ("\n");
}

const char *TableGetCountry (UWORD code)
{
    int i;

    assert (TableCountryCodesSize == TableCountrySize);

    if (!code)
        return i18n (-1, TableCountry[0]);

    for (i = 0; TableCountryCodes[i]; i++)
        if (TableCountryCodes[i] == code)
            return i18n (-1, TableCountry[i]);

    return i18n (199, "Unknown country");
}

const char *TableGetOccupation (UWORD code)
{
    if (code == 99)
        return _i18n (198, "Other Services");

    if (code >= TableOccupationSize)
        code = 0;

    return i18n (-1, TableOccupation[code]);
}

const char *TableGetInterest (UWORD code)
{
    code -= 100;
    if (code == 99)
        return _i18n (499, "Other");

    if (code >= TableInterestSize)
        return NULL;

    return i18n (-1, TableInterest[code]);
}
