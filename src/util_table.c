/*
 * Looks up (internationalized) strings from a table.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util_table.h"
#include "util_ui.h"
#include <assert.h>



static const char *TableMonths[] = {   /* *INDENT-OFF* */
    _i18n (1014, "None"),
    _i18n (1015, "Jan"),    _i18n (1016, "Feb"),    _i18n (1017, "Mar"),    _i18n (1018, "Apr"),
    _i18n (1019, "May"),    _i18n (1020, "Jun"),    _i18n (1021, "Jul"),    _i18n (1022, "Aug"),
    _i18n (1023, "Sep"),    _i18n (1024, "Oct"),    _i18n (1025, "Nov"),    _i18n (1026, "Dec")
};

static const char *TableLang[] = {
    _i18n (1100, "None"),
    _i18n (1101, "Arabic"),      _i18n (1102, "Bhojpuri"),    _i18n (1103, "Bulgarian"),       _i18n (1104, "Burmese"),
    _i18n (1105, "Cantonese"),   _i18n (1106, "Catalan"),     _i18n (1107, "Chinese"),         _i18n (1108, "Croatian"),
    _i18n (1109, "Czech"),       _i18n (1110, "Danish"),      _i18n (1111, "Dutch"),           _i18n (1112, "English"),
    _i18n (1113, "Esperanto"),   _i18n (1114, "Estonian"),    _i18n (1115, "Farsi"),           _i18n (1116, "Finnish"),
    _i18n (1117, "French"),      _i18n (1118, "Gaelic"),      _i18n (1119, "German"),          _i18n (1120, "Greek"),
    _i18n (1121, "Hebrew"),      _i18n (1122, "Hindi"),       _i18n (1123, "Hungarian"),       _i18n (1124, "Icelandic"),
    _i18n (1125, "Indonesian"),  _i18n (1126, "Italian"),     _i18n (1127, "Japanese"),        _i18n (1128, "Khmer"),
    _i18n (1129, "Korean"),      _i18n (1130, "Lao"),         _i18n (1131, "Latvian"),         _i18n (1132, "Lithuanian"),
    _i18n (1133, "Malay"),       _i18n (1134, "Norwegian"),   _i18n (1135, "Polish"),          _i18n (1136, "Portuguese"),
    _i18n (1137, "Romanian"),    _i18n (1138, "Russian"),     _i18n (1139, "Serbo-Croatian"),  _i18n (1140, "Slovak"),
    _i18n (1141, "Slovenian"),   _i18n (1142, "Somali"),      _i18n (1143, "Spanish"),         _i18n (1144, "Swahili"),
    _i18n (1145, "Swedish"),     _i18n (1146, "Tagalog"),     _i18n (1147, "Tartar"),          _i18n (1148, "Thai"),
    _i18n (1149, "Turkish"),     _i18n (1150, "Ukrainian"),   _i18n (1151, "Urdu"),            _i18n (1152, "Vietnamese"),
    _i18n (1153, "Yiddish"),     _i18n (1154, "Yoruba"),      _i18n (1155, "Afrikaans"),       _i18n (1156, "Bosnian"),
    _i18n (1157, "Persian"),     _i18n (1158, "Albanian"),    _i18n (1159, "Armenian")
};
#define TableLangSize sizeof (TableLang) / sizeof (const char *)

static const char *TableCountry[] = {
    _i18n (1200, "not entered"),
    _i18n (1201, "Afghanistan"), _i18n (1202, "Albania"),                       _i18n (1203, "Algeria"),
                                _i18n (1204, "American Samoa"),                _i18n (1205, "Andorra"),
    _i18n (1206, "Angola"),      _i18n (1207, "Anguilla"),                      _i18n (1208, "Antigua"),
                                _i18n (1209, "Argentina"),                     _i18n (1210, "Armenia"),
    _i18n (1211, "Aruba"),       _i18n (1212, "Ascention Island"),              _i18n (1213, "Australia"),
                                _i18n (1214, "Australian Antartic Territory"), _i18n (1215, "Austria"),
    _i18n (1216, "Azerbaijan"),  _i18n (1217, "Bahamas"),                       _i18n (1218, "Bahrain"),
                                _i18n (1219, "Bangladesh"),                    _i18n (1220, "Barbados"),
    _i18n (1221, "Belarus"),     _i18n (1222, "Belgium"),                       _i18n (1223, "Belize"),
                                _i18n (1224, "Benin"),                         _i18n (1225, "Bermuda"),
    _i18n (1226, "Bhutan"),      _i18n (1227, "Bolivia"),                       _i18n (1228, "Bosnia & Herzegovina"),
                                _i18n (1229, "Botswana"),                      _i18n (1230, "Brazil"),
    _i18n (1231, "British Virgin Islands"),   _i18n (1232, "Brunei"),           _i18n (1233, "Bulgaria"),
                                _i18n (1234, "Burkina Faso"),                  _i18n (1235, "Burundi"),
    _i18n (1236, "Cambodia"),    _i18n (1237, "Cameroon"),                      _i18n (1238, "Canada"),
                                _i18n (1239, "Cape Verde Islands"),            _i18n (1240, "Cayman Islands"),
    _i18n (1241, "Central African Republic"), _i18n (1242, "Chad"),             _i18n (1243, "Christmas Island"),
                                _i18n (1244, "Cocos-Keeling Islands"),         _i18n (1245, "Comoros"),
    _i18n (1246, "Congo"),       _i18n (1247, "Cook Islands"),                  _i18n (1248, "Chile"),
                                _i18n (1249, "China"),                         _i18n (1250, "Columbia"),
    _i18n (1251, "Costa Rice"),  _i18n (1252, "Croatia"),                       _i18n (1253, "Cuba"),
                                _i18n (1254, "Cyprus"),                        _i18n (1255, "Czech Republic"),
    _i18n (1256, "Denmark"),     _i18n (1257, "Diego Garcia"),                  _i18n (1258, "Djibouti"),
                                _i18n (1259, "Dominica"),                      _i18n (1260, "Dominican Republic"),
    _i18n (1261, "Ecuador"),     _i18n (1262, "Egypt"),                         _i18n (1263, "El Salvador"),
                                _i18n (1264, "Equatorial Guinea"),             _i18n (1265, "Eritrea"),
    _i18n (1266, "Estonia"),     _i18n (1267, "Ethiopia"),                      _i18n (1268, "F.Y.R.O.M. (Former Yugoslavia)"),
                                _i18n (1269, "Faeroe Islands"),                _i18n (1270, "Falkland Islands"),
    _i18n (1271, "Federated States of Micronesia"), _i18n (1272, "Fiji"),       _i18n (1273, "Finland"),
                                _i18n (1274, "France"),                        _i18n (1275, "French Antilles"),
    _i18n (1276, "French Antilles"),         _i18n (1277, "French Guiana"),     _i18n (1278, "French Polynesia"),
                                _i18n (1279, "Gabon"),                         _i18n (1280, "Gambia"),
    _i18n (1281, "Georgia"),     _i18n (1282, "Germany"),                       _i18n (1283, "Ghana"),       
                                _i18n (1284, "Gibraltar"),                     _i18n (1285, "Greece"),      
    _i18n (1286, "Greenland"),   _i18n (1287, "Grenada"),                       _i18n (1288, "Guadeloupe"),  
                                _i18n (1289, "Guam"),                          _i18n (1290, "Guantanomo Bay"),
    _i18n (1291, "Guatemala"),   _i18n (1292, "Guinea"),                        _i18n (1293, "Guinea-Bissau"),
                                _i18n (1294, "Guyana"),                        _i18n (1295, "Haiti"),       
    _i18n (1296, "Honduras"),    _i18n (1297, "Hong Kong"),                     _i18n (1298, "Hungary"),
                                _i18n (1299, "Iceland"),                       _i18n (1300, "India"),       
    _i18n (1301, "Indonesia"),   _i18n (1302, "INMARSAT"),                      _i18n (1303, "INMARSAT Atlantic-East"),
                                _i18n (1304, "Iran"),                          _i18n (1305, "Iraq"),        
    _i18n (1306, "Ireland"),     _i18n (1307, "Israel"),                        _i18n (1308, "Italy"),
                                _i18n (1309, "Ivory Coast"),                   _i18n (1310, "Japan"),       
    _i18n (1311, "Jordan"),      _i18n (1312, "Kazakhstan"),                    _i18n (1313, "Kenya"),
                                _i18n (1314, "South Korea"),                   _i18n (1315, "Kuwait"),      
    _i18n (1316, "Liberia"),     _i18n (1317, "Libya"),                         _i18n (1318, "Liechtenstein"),
                                _i18n (1319, "Luxembourg"),                    _i18n (1320, "Malawi"),      
    _i18n (1321, "Malaysia"),    _i18n (1322, "Mali"),                          _i18n (1323, "Malta"),
                                _i18n (1324, "Mexico"),                        _i18n (1325, "Monaco"),      
    _i18n (1326, "Morocco"),     _i18n (1327, "Namibia"),                       _i18n (1328, "Nepal"),       
                                _i18n (1329, "Netherlands"),                   _i18n (1330, "Netherlands Antilles"),
    _i18n (1331, "New Caledonia"),           _i18n (1332, "New Zealand"),       _i18n (1333, "Nicaragua"),
                                _i18n (1334, "Nigeria"),                       _i18n (1335, "Norway"),      
    _i18n (1336, "Oman"),        _i18n (1337, "Pakistan"),                      _i18n (1338, "Panama"),      
                                _i18n (1339, "Papua New Guinea"),              _i18n (1340, "Paraguay"),
    _i18n (1341, "Peru"),        _i18n (1342, "Philippines"),                   _i18n (1343, "Poland"),      
                                _i18n (1344, "Portugal"),                      _i18n (1345, "Puerto Rico"), 
    _i18n (1346, "Qatar"),       _i18n (1347, "Romania"),                       _i18n (1348, "Russia"),
                                _i18n (1349, "Saipan"),                        _i18n (1350, "San Marino"),  
    _i18n (1351, "Saudia Arabia"),           _i18n (1352, "Senegal"),           _i18n (1353, "Singapore"),
                                _i18n (1354, "Slovakia"),                      _i18n (1355, "South Africa"),
    _i18n (1356, "Spain"),       _i18n (1357, "Sri Lanka"),                     _i18n (1358, "Suriname"),
                                _i18n (1359, "Sweden"),                        _i18n (1360, "Switzerland"), 
    _i18n (1361, "Taiwan"),      _i18n (1362, "Tanzania"),                      _i18n (1363, "Thailand"),
                                _i18n (1364, "Tinian Island"),                 _i18n (1365, "Togo"),        
    _i18n (1366, "Tokelau"),     _i18n (1367, "Tonga"),                         _i18n (1368, "Trinadad and Tabago"),
                                _i18n (1369, "Tunisia"),                       _i18n (1370, "Turkey"),      
    _i18n (1371, "Turkmenistan"),_i18n (1372, "Turks and Caicos Islands"),      _i18n (1373, "Tuvalu"),
                                _i18n (1374, "Uganda"),                        _i18n (1375, "Ukraine"),     
    _i18n (1376, "United Arab Emirates"),    _i18n (1377, "UK"),                _i18n (1378, "United States Virgin Islands"),
                                _i18n (1379, "USA"),                           _i18n (1380, "Uruguay"),     
    _i18n (1381, "Uzbekistan"),  _i18n (1382, "Vanuatu"),                       _i18n (1383, "Vatican City"),
                                _i18n (1384, "Venezuela"),                     _i18n (1385, "Vietnam"),     
    _i18n (1386, "Wallis and Futuna Islands"), _i18n (1387, "Western Samoa"),   _i18n (1388, "Yemen"),
                                _i18n (1389, "Yugoslavia"),                    _i18n (1390, "Zaire"),       
    _i18n (1391, "Zambia"),     _i18n (1392, "Zimbabwe"),                      _i18n (2157, "Moldova"),
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
    709,   118,   688,   256,   380,        971,    44,   101,     1,   123,
    598,   711,   678,   378,   379,         58,    84,   681,   685,   967,
    381,   243,   373,
    0
};
#define TableCountryCodesSize sizeof (TableCountryCodes) / sizeof (UWORD)

static const char *TableOccupation[] = {
    _i18n (1200, "not entered"),
    _i18n (1161, "Academic"),                    _i18n (1162, "Administrative"),        _i18n (1163, "Art/Entertainment"),
    _i18n (1164, "College Student"),             _i18n (1165, "Computers"),             _i18n (1166, "Community & Social"),
    _i18n (1167, "Education"),                   _i18n (1168, "Engineering"),           _i18n (1169, "Financial Services"),
    _i18n (1170, "Government"),                  _i18n (1171, "High School Student"),   _i18n (1172, "Home"),
    _i18n (1173, "ICQ - Providing Help"),        _i18n (1174, "Law"),                   _i18n (1175, "Managerial"),
    _i18n (1176, "Manufacturing"),               _i18n (1177, "Medical/Health"),        _i18n (1178, "Military"),
    _i18n (1179, "Non-government Organization"), _i18n (1180, "Professional"),          _i18n (1181, "Retail"),
    _i18n (1182, "Retired"),                     _i18n (1183, "Science & Research"),    _i18n (1184, "Sports"),
    _i18n (1185, "Technical"),                   _i18n (1186, "University Student"),    _i18n (1187, "Web Building")
};
#define TableOccupationSize sizeof (TableOccupation) / sizeof (const char *)

static const char *TableInterest[] = {
    _i18n (1455, "Art"),                   _i18n (1456, "Cars"),                  _i18n (1457, "Celebrity Fans"),    
    _i18n (1458, "Collections"),           _i18n (1459, "Computers"),             _i18n (1460, "Culture & Literature"), 
    _i18n (1461, "Fitness"),               _i18n (1462, "Games"),                 _i18n (1463, "Hobbies"),           
    _i18n (1173, "ICQ - Providing Help"),  _i18n (1465, "Internet"),              _i18n (1466, "Lifestyle"),         
    _i18n (1467, "Movies/TV"),             _i18n (1468, "Music"),                 _i18n (1469, "Outdoor Activities"),
    _i18n (1470, "Parenting"),             _i18n (1471, "Pets/Animals"),          _i18n (1472, "Religion"),          
    _i18n (1473, "Science/Technology"),    _i18n (1474, "Skills"),                _i18n (1475, "Sports"),            
    _i18n (1476, "Web Design"),            _i18n (1477, "Nature and Environment"),_i18n (1478, "News & Media"),      
    _i18n (1479, "Government"),            _i18n (1480, "Business & Economy"),    _i18n (1481, "Mystics"),           
    _i18n (1482, "Travel"),                _i18n (1483, "Astronomy"),             _i18n (1484, "Space"),             
    _i18n (1485, "Clothing"),              _i18n (1486, "Parties"),               _i18n (1487, "Women"),             
    _i18n (1488, "Social science"),        _i18n (1489, "60's"),                  _i18n (1490, "70's"),              
    _i18n (1491, "80's"),                  _i18n (1492, "50's"),              
    _i18n (1797, "Finance and corporate"), _i18n (2000, "Entertainment"),
    _i18n (2001, "Consumer electronics"),  _i18n (2002, "Retail stores"),
    _i18n (2003, "Health and beauty"),     _i18n (2004, "Media"),
    _i18n (2005, "Household products"),    _i18n (2006, "Mail order catalog"),
    _i18n (2007, "Business services"),     _i18n (1977, "Audio and visual"),
    _i18n (1978, "Sporting and athletic"), _i18n (1979, "Publishing"),
    _i18n (1980, "Home automation")
};
#define TableInterestSize sizeof (TableInterest) / sizeof (const char *)

static const char *TableAffiliation[] = {
    _i18n (1981, "Alumni Org."),
    _i18n (1982, "Charity Org."),
    _i18n (1983, "Club/Social Org."),
    _i18n (1984, "Community Org."),
    _i18n (1985, "Cultural Org."),
    _i18n (1986, "Fan Clubs"),
    _i18n (1987, "Fraternity/Sorority"),
    _i18n (1988, "Hobbyists Org."),
    _i18n (1989, "International Org."),
    _i18n (1990, "Nature and Environment Org."),
    _i18n (1991, "Professional Org."),
    _i18n (1992, "Scientific/Technical Org."),
    _i18n (1993, "Self Improvement Group"),
    _i18n (1994, "Spiritual/Religious Org."),
    _i18n (1995, "Sports Org."),
    _i18n (1996, "Support Org."),
    _i18n (1997, "Trade and Business Org."),
    _i18n (1998, "Union"),
    _i18n (1999, "Volunteer Org."),
};

#define TableAffiliationSize sizeof (TableAffiliation) / sizeof (const char *)

static const char *TablePast[] = {
    _i18n (1798, "Elementary School"),
    _i18n (1799, "High School"),
    _i18n (1808, "College"),
    _i18n (1810, "University"),
    _i18n (1178, "Military"),
    _i18n (1812, "Past Work Place"),
    _i18n (1813, "Past Organization"),
};
#define TablePastSize sizeof (TablePast) / sizeof (const char *)

const char *TableGetMonth (int code)   /* *INDENT-ON* */
{
    if (code > 12)
        code = 0;

    return i18n (-1, TableMonths[code]);
}

const char *TableGetLang (UBYTE code)
{
    if (code >= TableLangSize)
        return i18n (1099, "Unknown language.");

    return i18n (-1, TableLang[code]);
}

void TablePrintLang (void)
{
    int i;
    const char *p;

    for (i = 1; i < TableLangSize; i++)
    {
        p = i18n (-1, TableLang[i]);

        M_printf ("%2d. %-7s", i, p);
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

    return NULL;
}

const char *TableGetAffiliation (UWORD code)
{
    code -= 200;
    if (code == 99)
        return _i18n (1499, "Other");

    if (code >= TableAffiliationSize)
        return NULL;

    return i18n (-1, TableAffiliation[code]);
}


const char *TableGetPast (UWORD code)
{
    code -= 300;
    if (code == 99)
        return _i18n (1499, "Other");

    if (code >= TablePastSize)
        return NULL;

    return i18n (-1, TablePast[code]);
}

const char *TableGetOccupation (UWORD code)
{
    if (code == 99)
        return _i18n (1198, "Other Services");

    if (code >= TableOccupationSize)
        code = 0;

    return i18n (-1, TableOccupation[code]);
}

const char *TableGetInterest (UWORD code)
{
    code -= 100;
    if (code == 99)
        return _i18n (1499, "Other");

    if (code >= TableInterestSize)
        return NULL;

    return i18n (-1, TableInterest[code]);
}
