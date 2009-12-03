var g_map_object = null;
function init_map() {
        var latlng = new google.maps.LatLng(52, 0);
        var options = {
                zoom: 8,
                center: latlng,
                mapTypeId: google.maps.MapTypeId.ROADMAP
        };
        var map = new google.maps.Map(document.getElementById('map_canvas'), options)
        g_map_object = map;
}

function populate_map() {
       $.getJSON('data/manifest.json', null, 
       function(data) { 
               $.each(data, function(uuid, entry) {        
                       where = entry['landing-location'];
                       when = entry['launch-time'];
                       console.log('Prediction: ' + uuid + ' (' + where.latitude + ',' + where.longitude + ')');

                       var launch_time = new Date();
                       launch_time.setUTCFullYear(when.year - 1);
                       launch_time.setUTCMonth(when.month);
                       launch_time.setUTCDate(when.day);
                       launch_time.setUTCHours(when.hour);
                       launch_time.setUTCMinutes(when.minute);
                       launch_time.setUTCSeconds(when.second);

                       var latlng = new google.maps.LatLng(
                                where.latitude, where.longitude);
                       var marker = new google.maps.Marker({
                                position: latlng,
                                map: g_map_object,
                                title: launch_time.format('%d/%b/%Y %H:%M:%S'),
                       });
               });
       }
       );
}

function generateLinks(uuid) {
        var html;
        html = '<a target="_new" href="predictions/' + uuid + '/log">log</a>, ';
        html += '<a href="predictions/' + uuid + '/output">output</a>, ';
        html += '<a href="predictions/' + uuid + '/output.kml">KML output</a>';
        return html;
}

function refreshPredictionTable() {
       // Clear any existing pred_table.
       predictions_table.fnClearTable();

       $.getJSON('list.cgi', null, 
       function(data) { 
               $.each(data, function() {
                       predictions_table.fnAddData([ this.creation, this.creation ], false);
               });
               predictions_table.fnDraw();
       }
       );
}

function POSIXtoDate(timestamp)
{
        var d = new Date();
        d.setTime(timestamp * 1000);
        return d.format('%d/%b/%Y %H:%M:%S')
}

$(document).ready(function() {
        init_map();
        populate_map();
});

// vim:et:ts=8:sw=8:autoindent
