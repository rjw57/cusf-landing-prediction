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

var g_predictions = [];
function add_new_prediction(prediction) {
        $('#prediction-running').fadeOut();
        var path = []
        var max_height = -10;
        var max_point = null;

        var last_point = null;
        $.each(prediction.output, function(idx, entry) {
                if(entry.length >= 4) {
                        var point = new google.maps.LatLng( 
                                parseFloat(entry[1]), parseFloat(entry[2])
                        );
                        if(parseFloat(entry[3]) > max_height) {
                                max_height = parseFloat(entry[3]);
                                max_point = point;
                        }
                        path.push(point);
                        last_point = point;
                }
        });

        var pred_record = {
                'data': prediction,
        };

        var path_polyline = new google.maps.Polyline({
                path: path,
                strokeColor: '#000000',
                strokeWeight: 3,
                strokeOpacity: 0.75,
        });
        path_polyline.setMap(g_map_object);
        pred_record['path_polyline'] = path_polyline;

        if(max_point != null) {
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
                pred_record['pop_marker'] = pop_marker;
        }

        if(last_point != null) {
                var last_icon = new google.maps.MarkerImage('images/marker-sm-red.png',
                        new google.maps.Size(11, 11),
                        new google.maps.Point(0, 0),
                        new google.maps.Point(5.5, 5.5));

                var last_marker = new google.maps.Marker({
                        position: last_point,
                        map: g_map_object,
                        icon: last_icon,
                        title: 'Landing location',
                });
                pred_record['last_marker'] = last_marker;
        }

        g_predictions.push(pred_record);
}

function clear_all() {
        $.each(g_predictions, function(idx, pred) {
                if(pred.path_polyline) { pred.path_polyline.setMap(null); }
                if(pred.pop_marker) { pred.pop_marker.setMap(null); }
                if(pred.last_marker) { pred.last_marker.setMap(null); }
        });
        g_predictions = [];
}

function new_prediction() {
        var np_button = $('#new-prediction-button');
        $('#new_prediction').css({
                'top': np_button.offset().top + np_button.outerHeight(),
                'left': np_button.offset().left + np_button.outerWidth() - 
                        $('#new_prediction').outerWidth(),
        });
        $('#new_prediction').toggle('blind', function() {
                if($(this).css('display') == 'none') {
                        // was hidden
                        $('#new-prediction-button span').removeClass('ui-icon-triangle-1-n');
                        $('#new-prediction-button span').addClass('ui-icon-triangle-1-s');
                } else {
                        // was shown
                        $('#new-prediction-button span').removeClass('ui-icon-triangle-1-s');
                        $('#new-prediction-button span').addClass('ui-icon-triangle-1-n');
                }
        });
}

$(document).ready(function() {
        init_map();

        $('form#scenario').ajaxForm({
                'beforeSubmit': function() { $('#prediction-running').fadeIn(); },
                'success': function(data) { 
                        $('#prediction-running').fadeOut();
                        add_new_prediction(data); 
                },
                'dataType': 'json',
        });
        $('form#scenario').submit(function() {
                return false; // stop normal browser submit
        });

        $('#new-prediction-button').click(new_prediction);
        $('#clear-all-button').click(clear_all);

        $('.lp-button').hover(
                function(){$(this).addClass('ui-state-hover');},
                function(){$(this).removeClass('ui-state-hover');}
        );
});

// vim:et:ts=8:sw=8:autoindent
