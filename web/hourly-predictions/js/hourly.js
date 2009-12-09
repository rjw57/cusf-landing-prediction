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

function prediction_entry_convert_date(when) {
        var time = new Date();
        time.setUTCFullYear(when.year);
        time.setUTCMonth(when.month - 1);
        time.setUTCDate(when.day);
        time.setUTCHours(when.hour);
        time.setUTCMinutes(when.minute);
        time.setUTCSeconds(when.second);
        return time;
}

var g_prediction_polylines = { };
function show_prediction(prediction) {
        if(g_prediction_polylines[prediction.uuid] != null) { return; }

        $.get('data/' + prediction.uuid + '/output.csv', null, function(data, textStatus) {
                var lines = data.split('\n');
                var path = [ ];
                var max_height = -10;
                var max_point = null;
                $.each(lines, function(idx, line) {
                        entry = line.split(',');
                        var point = new google.maps.LatLng( parseFloat(entry[1]), parseFloat(entry[2]) );
                        if(parseFloat(entry[3]) > max_height) {
                                max_height = parseFloat(entry[3]);
                                max_point = point;
                        }
                        path.push(point);
                });

                var path_polyline = new google.maps.Polyline({
                        path: path,
                        strokeColor: '#000000',
                        strokeWeight: 3,
                        strokeOpacity: 0.75,
                });
                path_polyline.setMap(g_map_object);

                var pop_icon = new google.maps.MarkerImage('images/pop-marker.png',
                        new google.maps.Size(16, 16),
                        new google.maps.Point(0, 0),
                        new google.maps.Point(8, 8));
                var pop_marker = new google.maps.Marker({
                        position: max_point,
                        map: g_map_object,
                        icon: pop_icon,
                        title: 'Baloon burst (max. altitude: ' + max_height + 'm)',
                });

                google.maps.event.addListener(path_polyline, 'click', function() { 
                        path_polyline.setMap( null );
                        pop_marker.setMap( null );
                        g_prediction_polylines[prediction.uuid] = null;
                });

                g_prediction_polylines[prediction.uuid] = path_polyline;
        }, 'text');
}

function populate_map() {
        $.getJSON('data/manifest.json', null, 
        function(data) { 
                var marker_image = new google.maps.MarkerImage('images/marker-sm-red.png',
                        new google.maps.Size(11,11),
                        new google.maps.Point(0,0), // origin
                        new google.maps.Point(5.5,5.5)); // anchor

                // extract the predictions to an array of uuid, entry pairs
                var predictions = [];
                $.each(data['predictions'], function(uuid, entry) { 
                        predictions.push( { 'uuid': uuid, 'entry': entry } );
                });

                // sort the predictions in order of date
                predictions.sort(function(a,b) {
                        var a_date = prediction_entry_convert_date(a.entry['launch-time']);
                        var b_date = prediction_entry_convert_date(b.entry['launch-time']);
                        if(a_date < b_date) { return -1; }
                        if(a_date > b_date) { return 1; }
                        return 0;
                });

                // Add each prediction to the map
                var prediction_coords = [];
                $.each(predictions, function(idx, prediction) {
                        // console.log(prediction);
                        var where = prediction.entry['landing-location'];
                        var launch_time = prediction_entry_convert_date(prediction.entry['launch-time']);
                        var landing_time = prediction_entry_convert_date(prediction.entry['landing-time']);
                        var latlng = new google.maps.LatLng(
                                 where.latitude, where.longitude);
                        var marker = new google.maps.Marker({
                                 position: latlng,
                                 map: g_map_object,
                                 icon: marker_image,
                                 title: launch_time.format('%d/%b/%Y %H:%M:%S'),
                        });
                        prediction_coords.push(latlng);

                        var info_window = new google.maps.InfoWindow({
                                content: 
                                        '<p><strong>Launch time:</strong> ' + launch_time.format('%d/%b/%Y %H:%M:%S') + '</p>' +
                                        '<p><strong>Landing time:</strong> ' + landing_time.format('%d/%b/%Y %H:%M:%S') + '</p>' +
                                        '<p><strong>Landing location:</strong> ' + where.latitude + '&deg;N ' + where.longitude + '&deg;E</p>' +
                                        '<p><a target="_new" href="data/' + prediction.uuid + '/">Raw output data</a> (opens in new window)</p>'
                        });
                        google.maps.event.addListener(marker, 'click', function() { 
                                show_prediction(prediction);
                        });
                        google.maps.event.addListener(marker, 'rightclick', function() { 
                                info_window.open(g_map_object, marker); 
                        });
                });

                // Plot a path for the predictions
                var pred_path = new google.maps.Polyline({
                        path: prediction_coords,
                        strokeColor: '#ff4444',
                        strokeOpacity: 0.5,
                        strokeWeight: 2,
                });
                pred_path.setMap(g_map_object);
 
                var template = data['scenario-template'];
                $('#launch-lat').text(template['launch-site'].latitude);
                $('#launch-lon').text(template['launch-site'].longitude);
                $('#launch-alt').text(template['launch-site'].altitude);
                $('#ascent-rate').text(template['altitude-model']['ascent-rate']);
                $('#descent-rate').text(template['altitude-model']['descent-rate']);
                $('#burst-alt').text(template['altitude-model']['burst-altitude']);
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
